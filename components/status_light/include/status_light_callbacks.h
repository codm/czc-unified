#ifndef CZC_STATUS_LIGHT_CALLBACKS_H_
#define CZC_STATUS_LIGHT_CALLBACKS_H_

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Night-mode time range. LEDs are switched off between start and end.
 *        A range crossing midnight (e.g. 22:00 → 06:00) is valid.
 */
typedef struct {
    uint8_t start_hour;
    uint8_t start_minute;
    uint8_t end_hour;
    uint8_t end_minute;
} night_mode_config_t;

/**
 * @brief LED / night-mode callbacks — filled by StatusLightManager.
 *        ctx is a StatusLightManager* cast to void*.
 *
 *        `led` is a `LedId` and `mode` a `LedOverride` cast to int.
 *        The same numeric values are used by the web frontend.
 */
typedef struct {
    esp_err_t (*set_override)   (void *ctx, int led, int mode);
    esp_err_t (*get_override)   (void *ctx, int led, int *mode_out);
    esp_err_t (*set_night_mode) (void *ctx, const night_mode_config_t *cfg);
    esp_err_t (*get_night_mode) (void *ctx, night_mode_config_t *out);
    esp_err_t (*set_clear_night_mode) (void *ctx);
    void *ctx;
} web_led_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif // CZC_STATUS_LIGHT_CALLBACKS_H_
