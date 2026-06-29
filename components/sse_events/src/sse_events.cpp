#include "sse_events_init.h"
#include "sse_events.hpp"

#include <cstring>
#include <cstdio>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "sse_events";

constexpr int MAX_CLIENTS         = 4;
constexpr int QUEUE_SIZE          = 16;
constexpr int PING_INTERVAL_TICKS = 20; // 20 * 1s

struct SseEvent {
    char event[32];
    char data[256];
};

static std::vector<httpd_req_t *> s_clients;
static SemaphoreHandle_t          s_mutex          = nullptr;
static QueueHandle_t              s_queue          = nullptr;
static char                       s_retained_state[320] = {};

static void send_to_all(const char *buf, size_t len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (auto it = s_clients.begin(); it != s_clients.end(); ) {
        if (httpd_resp_send_chunk(*it, buf, (ssize_t)len) != ESP_OK) {
            ESP_LOGW(TAG, "Client disconnected, removing");
            httpd_req_async_handler_complete(*it);
            it = s_clients.erase(it);
        } else {
            ++it;
        }
    }
    xSemaphoreGive(s_mutex);
}

static void sse_task(void *)
{
    SseEvent ev;
    char     buf[320];
    int      ping_counter = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        while (xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
            int len = snprintf(buf, sizeof(buf), "event: %s\ndata: %s\n\n", ev.event, ev.data);
            send_to_all(buf, (size_t)len);
        }

        if (++ping_counter >= PING_INTERVAL_TICKS) {
            ping_counter = 0;
            send_to_all(": ping\n\n", 8);
        }
    }
}

static void add_client(httpd_req_t *async_req)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if ((int)s_clients.size() >= MAX_CLIENTS) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "SSE client limit reached, rejecting");
        httpd_req_async_handler_complete(async_req);
        return;
    }

    s_clients.push_back(async_req);
    ESP_LOGI(TAG, "SSE client connected (%d total)", (int)s_clients.size());

    if (s_retained_state[0]) {
        httpd_resp_send_chunk(async_req, s_retained_state, strlen(s_retained_state));
    }

    xSemaphoreGive(s_mutex);
}

static esp_err_t sse_handler(httpd_req_t *req)
{
    httpd_req_t *async_req = nullptr;
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

extern "C" esp_err_t sse_events_init(httpd_handle_t server)
{
    s_mutex = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(QUEUE_SIZE, sizeof(SseEvent));
    if (!s_mutex || !s_queue) return ESP_ERR_NO_MEM;

    xTaskCreate(sse_task, "sse_task", 4096, nullptr, 5, nullptr);

    static const httpd_uri_t uri = {
        .uri      = "/events",
        .method   = HTTP_GET,
        .handler  = sse_handler,
        .user_ctx = nullptr,
    };
    return httpd_register_uri_handler(server, &uri);
}

// ---------------------------------------------------------------------------

static esp_err_t post(const char *event, const char *data)
{
    if (!s_queue) return ESP_ERR_INVALID_STATE;
    SseEvent ev;
    strlcpy(ev.event, event, sizeof(ev.event));
    strlcpy(ev.data,  data,  sizeof(ev.data));
    return (xQueueSend(s_queue, &ev, 0) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

namespace sse_events {

esp_err_t post_flash_progress(SseFlashTarget target, SseFlashPhase phase, int percent)
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
    char data[128];
    snprintf(data, sizeof(data),
             "{\"percent\":%d,\"phase\":\"%s\",\"target\":\"%s\"}",
             percent, phase_str, target_str);
    return post("flash_progress", data);
}

esp_err_t post_flash_complete(SseFlashTarget target, bool success, const char *error)
{
    const char *target_str = (target == SseFlashTarget::ESP) ? "esp" : "rcp";
    char data[128];
    if (success) {
        snprintf(data, sizeof(data),
                 "{\"target\":\"%s\",\"success\":true}", target_str);
    } else {
        snprintf(data, sizeof(data),
                 "{\"target\":\"%s\",\"success\":false,\"error\":\"%s\"}",
                 target_str, error ? error : "");
    }
    return post("flash_complete", data);
}

esp_err_t post_device_state(SseDeviceMode mode, const char *phase)
{
    const char *mode_str;
    switch (mode) {
        case SseDeviceMode::NORMAL:   mode_str = "normal";   break;
        case SseDeviceMode::SETUP:    mode_str = "setup";    break;
        case SseDeviceMode::FLASHING: mode_str = "flashing"; break;
        default:                      mode_str = "unknown";  break;
    }
    char data[128];
    if (phase) {
        snprintf(data, sizeof(data), "{\"mode\":\"%s\",\"phase\":\"%s\"}", mode_str, phase);
    } else {
        snprintf(data, sizeof(data), "{\"mode\":\"%s\"}", mode_str);
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snprintf(s_retained_state, sizeof(s_retained_state),
             "event: device_state\ndata: %s\n\n", data);
    xSemaphoreGive(s_mutex);

    return post("device_state", data);
}

} // namespace sse_events
