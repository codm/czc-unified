/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "network_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Firmware / device-mode callbacks — filled by AppController.
 *        ctx is an AppController* cast to void*.
 */
typedef struct {
    esp_err_t (*flash_rcp)        (void *ctx, const char *url);
    esp_err_t (*flash_esp)        (void *ctx, const char *url);
    esp_err_t (*set_mode)         (void *ctx, int mode);
    esp_err_t (*get_mode)         (void *ctx, int *mode_out);
    /** Returns 1 if device has been provisioned, 0 on first boot. */
    esp_err_t (*get_device_setup) (void *ctx, int *setup_out);
    void *ctx;
} web_firmware_callbacks_t;

/**
 * @brief Start the border router web server immediately.
 *
 *        Registers all REST handlers and serves the frontend from @p base_path.
 *        Both callback structs must remain valid for the lifetime of the server.
 *
 * @param[in] base_path  VFS path of the SPIFFS frontend files (e.g. "/spiffs")
 * @param[in] fw_cbs     Firmware / mode callbacks — filled by AppController
 * @param[in] net_cbs    Network config callbacks — filled by NetworkStateMachine
 */
void esp_br_web_start(const char *base_path,
                      const web_firmware_callbacks_t *fw_cbs,
                      const web_network_callbacks_t  *net_cbs);

#ifdef __cplusplus
}
#endif
