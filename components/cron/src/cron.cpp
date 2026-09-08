#include "cron.h"

#include <vector>
#include <utility>

#include "esp_log.h"
#include "time_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace 
{
    constexpr const char* TAG = "CRON";
    
    TaskHandle_t cron_task_handle {NULL};

    std::vector<std::pair<cron_timing_t, esp_err_t(*)()>> jobs;
    
    /**
     * Checks if a specific jobs entry matches with current time
     * 
     * @param[in] index Job vector index to be compared 
     * 
     * @return True if current time and jobs entry match - false otherwise 
     */
    bool needsToBeRun(uint8_t index) {
        struct tm currentTime {};
        TimeService::getCurrTime(currentTime); 

        cron_timing_t jobTime {jobs.at(index).first};

        if (jobTime.minute == currentTime.tm_min &&
            jobTime.hour == currentTime.tm_hour &&
            jobTime.dotm == currentTime.tm_mday &&
            jobTime.weekday == currentTime.tm_wday &&
            jobTime.month == currentTime.tm_mon)
            return true;
        
        return false;
    }
    
    /* Goes through jobs vector and calls those jobs which need to be run */
    void handleAllJobs() {
        for (size_t i = 0; i < jobs.size(); i++)
        {
            if (needsToBeRun(i)) {
                ESP_LOGD(TAG, "Running Job with the handle %d", i);
                jobs.at(i).second();
            }            
        }
    }
    
    constexpr int TASK_TIMEOUT_MS = 1000;
    constexpr uint8_t TASK_INTERVAL_S = 60;
    void cron_task(void* pvParameters) {
        TickType_t start{xTaskGetTickCount()};
        while (true)
        {
            if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(TASK_INTERVAL_S * 1000)) 
            {
                start = xTaskGetTickCount();

                handleAllJobs();
            }

            vTaskDelay(pdMS_TO_TICKS(TASK_TIMEOUT_MS));
        }
    }
}  

namespace Cron {
    esp_err_t init()
    {
        xTaskCreate(cron_task, "Cron_Task", 2 * 1024, nullptr, 5, &cron_task_handle);

        return (cron_task_handle) ? ESP_OK : ESP_FAIL;
    }

    int scheduleJob(cron_timing_t cron_timing, esp_err_t (*funcptr)()) {
        jobs.push_back(std::make_pair(cron_timing, funcptr));

        return jobs.size();
    }

    esp_err_t removeJob(int job_handle) {
        jobs.erase(jobs.begin() + job_handle);

        return ESP_OK;
    }

    void close() {
        if (cron_task_handle)
            vTaskDelete(cron_task_handle);
    }

} /* Cron */
