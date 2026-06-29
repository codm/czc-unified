#ifndef SSE_EVENTS_HPP_
#define SSE_EVENTS_HPP_

#include "esp_err.h"

enum class SseFlashTarget { ESP, RCP };

enum class SseFlashPhase {
    DOWNLOADING,
    WRITING,
    REBOOTING,
    VERIFYING,
};

enum class SseDeviceMode { NORMAL, SETUP, FLASHING };

namespace sse_events {

esp_err_t post_flash_progress(SseFlashTarget target, SseFlashPhase phase, int percent);
esp_err_t post_flash_complete(SseFlashTarget target, bool success, const char *error = nullptr);
esp_err_t post_device_state(SseDeviceMode mode, const char *phase = nullptr);

} // namespace sse_events

#endif // SSE_EVENTS_HPP_
