#ifndef CZC_FIRMWARE_CALLBACKS_H_
#define CZC_FIRMWARE_CALLBACKS_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Firmware / device-mode callbacks — filled by AppController.
 *        ctx is an AppController* cast to void*.
 *
 *        Lives here (firmware_manager) so both esp_ot_br_server and
 *        app_controller can include it without a circular dependency,
 *        mirroring the web_network_callbacks_t pattern in network_config.h.
 */
typedef struct {
    /** Schedule RCP firmware update (writes URL, target mode, pending flag to NVS, reboots).
     *  The mode is applied to device_mode after the flash completes on the next boot. */
    esp_err_t (*flash_rcp)        (void *ctx, const char *url, int mode);

    /** Start a live ESP OTA update. */
    esp_err_t (*flash_esp)        (void *ctx, const char *url);

    /** Switch device mode (writes mode + matching RCP URL to NVS, reboots). */
    esp_err_t (*set_mode)         (void *ctx, int mode);

    /** Return the currently active device mode. */
    esp_err_t (*get_mode)         (void *ctx, int *mode_out);

    /** Returns 1 if device has been provisioned, 0 on first boot. */
    esp_err_t (*get_device_setup) (void *ctx, int *setup_out);

    void (*esp_reboot) (void *ctx);

    esp_err_t (*esp_erase_nvs) (void *ctx);

    esp_err_t (*rcp_reboot) (void *ctx);

    esp_err_t (*rcp_erase_nvram) (void *ctx);

    void (*esp_set_log_level) (void *ctx, int log_level);

    void *ctx;
} web_firmware_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif // CZC_FIRMWARE_CALLBACKS_H_
