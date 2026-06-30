#ifndef SSE_EVENTS_INIT_H_
#define SSE_EVENTS_INIT_H_

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialises the SSE event queue, sender task and registers the /events URI handler.
 *
 * Must be called once after the HTTP server has been started.
 * 
 * Creates Static Sse_events Object
 *
 * @param[in] server  Handle of the running HTTP server.
 * @return `ESP_OK` on success
 */
esp_err_t sse_events_init(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // SSE_EVENTS_INIT_H_
