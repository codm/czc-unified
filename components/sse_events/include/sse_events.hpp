#ifndef SSE_EVENTS_HPP_
#define SSE_EVENTS_HPP_

#include "esp_err.h"
#include <vector>
#include "freertos/queue.h"
#include "freertos/semphr.h"

enum class SseFlashTarget { ESP, RCP };

enum class SseFlashPhase {
    DOWNLOADING,
    WRITING,
    REBOOTING,
    VERIFYING,
};

enum class SseDeviceMode { NORMAL, SETUP, FLASHING };

class Sse_events
{
private:
    httpd_handle_t server;
    std::vector<httpd_req_t *> s_clients;
    SemaphoreHandle_t s_mutex;
    QueueHandle_t event_queue;
    TaskHandle_t send_task;

    /**
     * @brief SSE URI Handler - Opens async Eventstream and adds client to Queue
     * 
     * @return ESP_OK on success
     */
    static esp_err_t sse_handler(httpd_req_t* req);
    
    /**
     * @brief Adds client to client vector
     * 
     * @param[in] async_req Client / Request from sse handler
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` if MAX_CLIENTS reached 
     */
    esp_err_t add_client(httpd_req_t* async_req);

    static void sse_task(void* pv_parameters);

    /**
     * @brief Sends given Sse Event to all Clients in Vector
     * 
     * @param[in] buffer char* to SSE Event content in format: "event: ...\ndata:...\n\n"
     * @param[in] len length of buffer
     * 
     * @note buffer can be created via snprintf
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else
     */
    esp_err_t send_to_all(const char* buffer, size_t len);

public:
    Sse_events(httpd_handle_t _server);
    ~Sse_events();

    /**
     * @brief Registers URI Handler and starts event processing
     * 
     * @return `ESP_OK` on success - Else step specific error code 
     */
    esp_err_t init();
};

#endif // SSE_EVENTS_HPP_
