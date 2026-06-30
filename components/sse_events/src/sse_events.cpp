#include "sse_events_init.h"
#include "sse_events.hpp"

#include <cstring>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "Sse_events";

constexpr uint8_t MAX_CLIENTS {4};
constexpr uint8_t QUEUE_SIZE {16};
constexpr uint8_t PING_INTERVAL_S {20}; // 20 * 1s

constexpr uint16_t SSE_EVENT_SIZE {32};
constexpr uint16_t SSE_DATA_SIZE {256};
constexpr uint16_t SSE_SIZE = SSE_EVENT_SIZE + SSE_DATA_SIZE;

struct SseEvent {
    char event[SSE_EVENT_SIZE];
    char data[SSE_DATA_SIZE];
};

Sse_events::Sse_events(httpd_handle_t _server)
: server(_server),
s_clients(),
s_mutex(xSemaphoreCreateMutex()),
event_queue(xQueueCreate(QUEUE_SIZE, sizeof(SseEvent))),
send_task(nullptr)
{
}

Sse_events::~Sse_events()
{
    vSemaphoreDelete(s_mutex);
    vQueueDelete(event_queue);
    if (send_task)
    vTaskDelete(send_task);
}

esp_err_t Sse_events::init()
{
    static const httpd_uri_t uri = {
        .uri      = "/events",
        .method   = HTTP_GET,
        .handler  = sse_handler,
        .user_ctx = this,
    };
    esp_err_t ret = httpd_register_uri_handler(server, &uri);
    if (ret != ESP_OK)
    {
        ESP_LOGD(TAG, "Error registering URI Hanlder");
        return ESP_FAIL;
    }
    
    if (xTaskCreate(sse_task, "sse_task", 4 * 1024, this, 5, &send_task) != pdPASS) {
        ESP_LOGE(TAG, "Could not create SSE Send Task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t Sse_events::sse_handler(httpd_req_t *req)
{
    auto self = static_cast<Sse_events *>(req->user_ctx);

    httpd_req_t *async_req = nullptr;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(async_req, "text/event-stream");
    httpd_resp_set_hdr(async_req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(async_req, "Connection", "keep-alive");
    httpd_resp_send_chunk(async_req, ": ok\n\n", 6);

    self->add_client(async_req);
    return ESP_OK;
}

esp_err_t Sse_events::add_client(httpd_req_t *async_req)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_clients.size() >= MAX_CLIENTS)
    {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "SSE client limit reached, rejecting");
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }

    s_clients.push_back(async_req);
    ESP_LOGD(TAG, "SSE client connected (%d total)", s_clients.size());

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

void Sse_events::sse_task(void *pv_parameters)
{
    auto self = static_cast<Sse_events*>(pv_parameters);
    
    SseEvent event;
    uint8_t ping_counter {0};
    char buffer[SSE_SIZE];

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        while (xQueueReceive(self->event_queue, &event, 0) == pdTRUE)
        {
            int len = snprintf(buffer, sizeof(buffer), "event: %s\ndata: %s\n\n", event.event, event.data);
            self->send_to_all(buffer, len);
        }
        
        if (++ping_counter >= PING_INTERVAL_S) {
            ping_counter = 0;
            self->send_to_all(": ping\n\n", 8);
        }

    }
    
}

esp_err_t Sse_events::send_to_all(const char *buffer, size_t len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (auto it = s_clients.begin(); it != s_clients.end(); ) {
        if (httpd_resp_send_chunk(*it, buffer, (ssize_t)len) != ESP_OK) {
            ESP_LOGW(TAG, "Client disconnected, removing");
            httpd_req_async_handler_complete(*it);
            it = s_clients.erase(it);
        } else {
            ++it;
        }
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}