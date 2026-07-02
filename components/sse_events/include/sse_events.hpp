#ifndef SSE_EVENTS_HPP_
#define SSE_EVENTS_HPP_

#include "esp_err.h"
#include "esp_http_server.h"

enum class SseFlashTarget { ESP, RCP };

enum class SseFlashPhase {
    DOWNLOADING,
    WRITING,
    REBOOTING,
    VERIFYING,
};

enum class SseDeviceMode { NORMAL, SETUP, FLASHING };

namespace Sse_events 
{
    /**
     * @brief Initializes the SSE subsystem and registers the /events URI handler.
     *
     *        Creates the mutex, event queue and FreeRTOS dispatch task, then
     *        registers an HTTP GET handler at "/events" on the given server handle.
     *
     * @param[in] server  Handle to the running ESP HTTP server instance.
     *
     * @return `ESP_OK` on success,
     *         `ESP_ERR_NO_MEM` if mutex or queue allocation fails,
     *         `ESP_FAIL` if the FreeRTOS task could not be created,
     *         or any error code forwarded from `httpd_register_uri_handler`.
     */
    esp_err_t init(httpd_handle_t server);

    namespace flash
    {
        /**
         * @brief Posts a flash-progress event to all connected SSE clients.
         *
         *        Serializes target, phase and percent into a JSON payload and enqueues
         *        it as a "flash_progress" SSE event.
         *
         * @param[in] target   Which chip is being flashed (ESP or RCP).
         * @param[in] phase    Current phase of the flash process.
         * @param[in] percent  Progress percentage (0,00–100.00) with %.2f precision.
         *
         * @return `ESP_OK` if the event was enqueued,
         *         `ESP_FAIL` if the queue is full,
         *         `ESP_ERR_INVALID_STATE` if the subsystem is not initialized.
         */
        esp_err_t post_flash_progress(SseFlashTarget target, SseFlashPhase phase, float percent);

        /**
         * @brief Posts a flash-complete event to all connected SSE clients.
         *
         *        Serializes target, success flag and an optional error message into a
         *        JSON payload and enqueues it as a "flash_complete" SSE event.
         *
         * @param[in] target   Which chip was flashed (ESP or RCP).
         * @param[in] success  `true` if flashing succeeded, `false` otherwise.
         * @param[in] error    Optional error description; ignored when success is `true`.
         *
         * @return `ESP_OK` if the event was enqueued,
         *         `ESP_FAIL` if the queue is full,
         *         `ESP_ERR_INVALID_STATE` if the subsystem is not initialized.
         */
        esp_err_t post_flash_complete(SseFlashTarget target, bool success, const char *error = nullptr);

        /**
         * @brief Posts a device-state event to all connected SSE clients.
         *
         *        Serializes mode and an optional phase string into a JSON payload and
         *        enqueues it as a "device_state" SSE event.
         *
         * @param[in] mode   Current operating mode of the device.
         * @param[in] phase  Optional sub-phase description (e.g. "ota_check");
         *                   pass `nullptr` when not applicable.
         *
         * @return `ESP_OK` if the event was enqueued,
         *         `ESP_FAIL` if the queue is full,
         *         `ESP_ERR_INVALID_STATE` if the subsystem is not initialized.
         */
        esp_err_t post_device_state(SseDeviceMode mode, const char *phase = nullptr);
    } // namespace flash 

    namespace network 
    {
        /**
         * @brief Post a network-state-change event to all connected SSE clients
         * 
         * @param[in]
         */

    } // namespace network

} // namespace Sse_events

#endif // SSE_EVENTS_HPP_
