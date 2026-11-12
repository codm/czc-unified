#include "sse_events.hpp"

#include <cstring>
#include <cstdio>
#include <vector>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "Sse_events";

constexpr uint8_t  MAX_CLIENTS {4};
constexpr uint8_t  QUEUE_SIZE {16};
constexpr uint8_t  PING_INTERVAL_S {20};

constexpr uint16_t SSE_EVENT_SIZE {32};
constexpr uint16_t SSE_DATA_SIZE {256};
constexpr uint16_t SSE_SIZE = SSE_EVENT_SIZE + SSE_DATA_SIZE;

struct SseEvent {
    char event[SSE_EVENT_SIZE];
    char data[SSE_DATA_SIZE];
};

static std::vector<httpd_req_t *> s_clients;
static SemaphoreHandle_t s_mutex {nullptr};
static QueueHandle_t s_queue {nullptr};
static char s_cached_device_state_frame[SSE_SIZE] {0};
static std::atomic<TaskHandle_t> s_sse_task_handle {nullptr};
static std::atomic<bool> s_stopping {false};
static std::atomic<uint32_t> s_active_operations {0};
static httpd_handle_t s_http_server {nullptr};

class SseOperation
{
public:
    SseOperation()
    {
        if (s_stopping.load()) {
            return;
        }

        s_active_operations.fetch_add(1);
        if (!s_stopping.load()) {
            m_active = true;
        } else {
            s_active_operations.fetch_sub(1);
        }
    }

    ~SseOperation()
    {
        if (m_active) {
            s_active_operations.fetch_sub(1);
        }
    }

    explicit operator bool() const { return m_active; }

private:
    bool m_active {false};
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief Sends a raw buffer to all connected SSE clients, pruning dead connections.
 *
 *        Acquires the client mutex, iterates over s_clients and calls
 *        `httpd_resp_send_chunk` for each entry. Clients for which the send
 *        fails are completed via `httpd_req_async_handler_complete` and removed.
 *
 * @param[in] buffer  Pointer to the data to send.
 * @param[in] len     Number of bytes to send.
 */
static void send_to_all(const char *buffer, size_t len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (auto it = s_clients.begin(); it != s_clients.end(); ) {
        if (httpd_resp_send_chunk(*it, buffer, static_cast<ssize_t>(len)) != ESP_OK) {
            ESP_LOGW(TAG, "Client disconnected, removing");
            httpd_req_async_handler_complete(*it);
            it = s_clients.erase(it);
        } else {
            ++it;
        }
    }
    xSemaphoreGive(s_mutex);
}

/**
 * @brief Sends an SseEvent to one specific client.
 * 
 * @param[in] client  Pointer to the specific client.
 * @param[in] buffer  Pointer to the data to send.
 * @param[in] len     Number of bytes to send.
 * 
 * @return `ESP_OK` on success - `ESP_FAIL` if client could not be reached
 */
static esp_err_t send_to_single_client(httpd_req_t* client, const char* buffer, size_t len)
{
    if (httpd_resp_send_chunk(client, buffer, static_cast<ssize_t>(len)) != ESP_OK)
    {
        ESP_LOGW(TAG, "Client could not be reached!");
        httpd_req_async_handler_complete(client);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief FreeRTOS task that drains the event queue and sends periodic keep-alive pings.
 *
 *        Wakes every second, flushes all pending `SseEvent` entries from
 *        `s_queue` by formatting them into SSE frames and forwarding them to
 *        `send_to_all`. Every `PING_INTERVAL_S` seconds a comment frame is sent
 *        to prevent proxies and browsers from closing idle connections.
 *
 * @param[in] pvParameters  Unused task parameter (required by FreeRTOS signature).
 */
static void sse_task(void *)
{
    SseEvent ev;
    char buffer[SSE_SIZE];
    uint8_t ping_counter {0};

    while (!s_stopping) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (s_stopping) break;

        while (xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
            if (s_stopping) break;
            int len = snprintf(buffer, sizeof(buffer), "event: %s\ndata: %s\n\n", ev.event, ev.data);
            if (len > 0) {
                send_to_all(buffer, static_cast<size_t>(len) < sizeof(buffer)
                                        ? static_cast<size_t>(len)
                                        : sizeof(buffer) - 1);
            }
        }

        if (++ping_counter >= PING_INTERVAL_S) {
            ping_counter = 0;
            if (!s_stopping) {
                send_to_all(": ping\n\n", 8);
            }
        }
    }
    s_sse_task_handle.store(nullptr);
    vTaskDelete(nullptr);
}

/**
 * @brief Adds an asynchronous HTTP request handle to the active client list.
 *
 *        Acquires the client mutex and appends `async_req` to `s_clients` if
 *        `MAX_CLIENTS` has not been reached. If the limit is exceeded the
 *        request is immediately completed and the connection is rejected.
 *
 * @param[in] async_req  Asynchronous request handle obtained from
 *                       `httpd_req_async_handler_begin`.
 */
static void add_client(httpd_req_t *async_req)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_clients.size() >= MAX_CLIENTS) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "SSE client limit reached, rejecting");
        httpd_req_async_handler_complete(async_req);
        return;
    }

    if (s_cached_device_state_frame[0] != '\0') {
        esp_err_t ret = send_to_single_client(async_req, s_cached_device_state_frame, strlen(s_cached_device_state_frame));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "New client unreachable, dropping connection");
            xSemaphoreGive(s_mutex);
            return;
        }
    }

    s_clients.push_back(async_req);
    ESP_LOGD(TAG, "SSE client connected (%d total)", (int)s_clients.size());

    xSemaphoreGive(s_mutex);
}

/**
 * @brief ESP-IDF HTTP URI handler for the SSE endpoint ("/events").
 *
 *        Converts the incoming synchronous request to an asynchronous handle,
 *        sets the required SSE response headers (Content-Type, Cache-Control,
 *        Connection), sends an initial confirmation comment and registers the
 *        connection via `add_client`.
 *
 * @param[in] req  Incoming HTTP request from the ESP-IDF HTTP server.
 *
 * @return `ESP_OK` on success,
 *         `ESP_FAIL` if the async upgrade fails.
 */
static esp_err_t sse_handler(httpd_req_t *req)
{
    SseOperation operation;
    if (!operation) {
        return ESP_FAIL;
    }

    httpd_req_t *async_req {nullptr};
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(async_req, "text/event-stream");
    httpd_resp_set_hdr(async_req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(async_req, "Connection", "keep-alive");
    httpd_resp_send_chunk(async_req, ": ok\n\n", 6);

    add_client(async_req);
    return ESP_OK;
}

/**
 * @brief Enqueues a named SSE event for delivery to all connected clients.
 *
 *        Copies event name and data into a `SseEvent` struct and posts it to
 *        `s_queue` in a non-blocking fashion (timeout = 0).
 *
 * @param[in] event  Null-terminated event name (max `SSE_EVENT_SIZE - 1` chars).
 * @param[in] data   Null-terminated event data (max `SSE_DATA_SIZE - 1` chars).
 *
 * @return `ESP_OK` if the event was enqueued,
 *         `ESP_FAIL` if the queue is full,
 *         `ESP_ERR_INVALID_STATE` if `s_queue` is not initialized.
 */
static esp_err_t post(const char *event, const char *data)
{
    SseOperation operation;
    if (!operation || !s_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    SseEvent ev;
    strlcpy(ev.event, event, sizeof(ev.event));
    strlcpy(ev.data,  data,  sizeof(ev.data));
    return (xQueueSend(s_queue, &ev, 0) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

// ---------------------------------------------------------------------------
// Public namespace API
// ---------------------------------------------------------------------------

namespace Sse_events {

esp_err_t init(httpd_handle_t server)
{
    deinit();

    s_stopping = false;
    s_mutex = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(QUEUE_SIZE, sizeof(SseEvent));
    if (!s_mutex || !s_queue) {
        if (s_mutex) {
            vSemaphoreDelete(s_mutex);
            s_mutex = nullptr;
        }
        if (s_queue) {
            vQueueDelete(s_queue);
            s_queue = nullptr;
        }
        return ESP_ERR_NO_MEM;
    }

    static const httpd_uri_t uri = {
        .uri     = "/events",
        .method  = HTTP_GET,
        .handler = sse_handler,
        .user_ctx = nullptr,
    };
    if (httpd_register_uri_handler(server, &uri) != ESP_OK) {
        ESP_LOGE(TAG, "Error registering URI handler");
        vSemaphoreDelete(s_mutex);
        vQueueDelete(s_queue);
        s_mutex = nullptr;
        s_queue = nullptr;
        return ESP_FAIL;
    }
    s_http_server = server;

    TaskHandle_t task_handle {nullptr};
    if (xTaskCreate(sse_task, "sse_task", 4 * 1024, nullptr, 5, &task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Could not create SSE send task");
        httpd_unregister_uri_handler(server, "/events", HTTP_GET);
        s_http_server = nullptr;
        s_sse_task_handle = nullptr;
        vSemaphoreDelete(s_mutex);
        vQueueDelete(s_queue);
        s_mutex = nullptr;
        s_queue = nullptr;
        return ESP_FAIL;
    }
    s_sse_task_handle.store(task_handle);

    return ESP_OK;
}

void deinit()
{
    if (s_stopping && s_mutex == nullptr && s_queue == nullptr && s_sse_task_handle.load() == nullptr) {
        return;
    }

    s_stopping = true;

    if (s_http_server != nullptr) {
        httpd_unregister_uri_handler(s_http_server, "/events", HTTP_GET);
        s_http_server = nullptr;
    }

    while (s_sse_task_handle.load() != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (s_active_operations.load() != 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (s_mutex != nullptr) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        for (auto *client : s_clients) {
            httpd_req_async_handler_complete(client);
        }
        s_clients.clear();
        xSemaphoreGive(s_mutex);

        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
    }

    if (s_queue != nullptr) {
        vQueueDelete(s_queue);
        s_queue = nullptr;
    }

    s_cached_device_state_frame[0] = '\0';

}

esp_err_t flash::post_flash_progress(SseFlashTarget target, SseFlashPhase phase, float percent)
{
    const char *target_str = (target == SseFlashTarget::ESP) ? "esp" : "rcp";
    const char *phase_str;
    switch (phase) {
        case SseFlashPhase::DOWNLOADING: phase_str = "downloading"; break;
        case SseFlashPhase::WRITING:     phase_str = "writing";     break;
        case SseFlashPhase::REBOOTING:   phase_str = "rebooting";   break;
        case SseFlashPhase::VERIFYING:   phase_str = "verifying";   break;
        default:                         phase_str = "unknown";     break;
    }
    char data[SSE_DATA_SIZE];
    snprintf(data, sizeof(data),
             "{\"percent\":%.2f,\"phase\":\"%s\",\"target\":\"%s\"}",
             percent, phase_str, target_str);
    return post("flash_progress", data);
}

esp_err_t flash::post_flash_complete(SseFlashTarget target, bool success, const char *error)
{
    const char *target_str = (target == SseFlashTarget::ESP) ? "esp" : "rcp";
    char data[SSE_DATA_SIZE];
    if (success) {
        snprintf(data, sizeof(data), "{\"target\":\"%s\",\"success\":true}", target_str);
    } else {
        snprintf(data, sizeof(data),
                 "{\"target\":\"%s\",\"success\":false,\"error\":\"%s\"}",
                 target_str, error ? error : "");
    }
    return post("flash_complete", data);
}

esp_err_t flash::post_device_state(SseDeviceMode mode, const char *phase)
{
    SseOperation operation;
    if (!operation) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *mode_str;
    switch (mode)
    {
    case SseDeviceMode::NORMAL:   mode_str = "normal";   break;
    case SseDeviceMode::SETUP:    mode_str = "setup";    break;
    case SseDeviceMode::FLASHING: mode_str = "flashing"; break;
    default:                      mode_str = "unknown";  break;
    }
    char data[SSE_DATA_SIZE];
    if (phase)
        snprintf(data, sizeof(data), "{\"mode\":\"%s\",\"phase\":\"%s\"}", mode_str, phase);
    else
        snprintf(data, sizeof(data), "{\"mode\":\"%s\"}", mode_str);

    if (s_mutex != nullptr) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        snprintf(s_cached_device_state_frame, sizeof(s_cached_device_state_frame),
                 "event: device_state\ndata: %s\n\n", data);
        xSemaphoreGive(s_mutex);
    }
    return post("device_state", data);
}
esp_err_t network::post_network_state_change(int mode, const char *ip)
{
    char data[SSE_DATA_SIZE];
    snprintf(data, sizeof(data), "{\"mode\":%d,\"ip\":\"%s\"}", mode, ip ? ip : "");

    return post("network_state_change", data);
}

} // namespace Sse_events
