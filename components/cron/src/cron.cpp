#include "cron.h"

#include <vector>
#include <tuple>

#include "esp_log.h"
#include "time_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

namespace 
{
    constexpr const char* TAG = "CRON";

    uint8_t job_handle_counter {0};
    SemaphoreHandle_t jobs_access_handle {NULL};

    TaskHandle_t cron_task_handle {NULL};

    std::vector<std::tuple<uint8_t, cron_timing_t, esp_err_t(*)(void* ctx), void*>> jobs;
    
    /* Field matches if it is a wildcard or equals the current value */
    bool fieldMatches(uint8_t field, int current) {
        return field == CRON_ANY || field == current;
    }

    /**
     * Checks if a specific jobs entry matches with the given time
     *
     * @param[in] index Job vector index to be compared
     * @param[in] currentTime Time to compare against
     *
     * @return True if time and jobs entry match - false otherwise
     */
    bool needsToBeRun(size_t index, const struct tm& currentTime) {
        cron_timing_t jobTime {std::get<1>(jobs.at(index))};

        return fieldMatches(jobTime.minute, currentTime.tm_min) &&
               fieldMatches(jobTime.hour, currentTime.tm_hour) &&
               fieldMatches(jobTime.dotm, currentTime.tm_mday) &&
               fieldMatches(jobTime.weekday, currentTime.tm_wday) &&
               fieldMatches(jobTime.month, currentTime.tm_mon);
    }

    /* Goes through jobs vector and calls those jobs which need to be run */
    void handleAllJobs(const struct tm& currentTime) {
        for (size_t i = 0; i < jobs.size(); i++)
        {
            if (needsToBeRun(i, currentTime)) {
                ESP_LOGD(TAG, "Running Job with the handle %d", std::get<0>(jobs.at(i)));
                esp_err_t(*funcptr)(void* ctx) = std::get<2>(jobs.at(i));
                esp_err_t ret {funcptr(std::get<3>(jobs.at(i)))};
                if (ret != ESP_OK)
                    ESP_LOGW(TAG, "Non ESP_OK return value of Cron job with handle %d", std::get<0>(jobs.at(i)));
            }
        }
    }

    constexpr int SECONDS_PER_MINUTE = 60;
    /* Margin so the wakeup lands safely behind the minute boundary */
    constexpr int WAKEUP_MARGIN_MS = 100;

    /* Task wakes once per minute right after the minute boundary and runs all matching jobs */
    void cron_task(void* pvParameters) {
        int lastMinute {-1};
        while (true)
        {
            struct tm now {};
            /* Skip jobs while time is not synced yet - otherwise they match against 1970 */
            if (TimeService::getCurrTime(now) == ESP_OK && now.tm_min != lastMinute)
            {
                lastMinute = now.tm_min;

                xSemaphoreTake(jobs_access_handle, portMAX_DELAY);
                handleAllJobs(now);
                xSemaphoreGive(jobs_access_handle);
            }

            /* Re-read after jobs so their runtime is not added to the delay */
            TimeService::getCurrTime(now);
            int delayMs {(SECONDS_PER_MINUTE - now.tm_sec) * 1000 + WAKEUP_MARGIN_MS};
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
}  

namespace Cron {
    esp_err_t init()
    {
        jobs_access_handle = xSemaphoreCreateMutex();
        if (jobs_access_handle == NULL) {
            ESP_LOGE(TAG, "init: Error creating mutex");
            return ESP_FAIL;
        }

        xTaskCreate(cron_task, "Cron_Task", 2 * 1024, nullptr, 5, &cron_task_handle);

        return (cron_task_handle) ? ESP_OK : ESP_FAIL;
    }

    int scheduleJob(cron_timing_t cron_timing, esp_err_t (*funcptr)(void* ctx), void* ctx) {
        xSemaphoreTake(jobs_access_handle,portMAX_DELAY);
        jobs.push_back(std::make_tuple(job_handle_counter, cron_timing, funcptr, ctx));
        xSemaphoreGive(jobs_access_handle);

        job_handle_counter++;

        return (job_handle_counter - 1);
    }

    esp_err_t removeJob(int job_handle) {
        xSemaphoreTake(jobs_access_handle, portMAX_DELAY);
        for (size_t i = 0; i < jobs.size(); i++) {
            if (std::get<0>(jobs.at(i)) == job_handle) {
                jobs.erase(jobs.begin() + i);
                xSemaphoreGive(jobs_access_handle);
                return ESP_OK;
            }
        }
        xSemaphoreGive(jobs_access_handle);

        ESP_LOGW(TAG, "Couldn't remove job with handle: %d - not found!", job_handle);
        return ESP_ERR_INVALID_ARG;
    }

    void close()
    {
        if (cron_task_handle)
            vTaskDelete(cron_task_handle);
        
        if(jobs_access_handle)
            vSemaphoreDelete(jobs_access_handle);
    }

} /* Cron */
