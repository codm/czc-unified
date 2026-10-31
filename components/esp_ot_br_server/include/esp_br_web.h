/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "firmware_callbacks.h"
#include "network_config.h"
#include "status_light_callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the border router web server immediately.
 *
 *        Registers all REST handlers and serves the frontend from @p base_path.
 *        Both callback structs must remain valid for the lifetime of the server.
 *
 * @param[in] base_path  VFS path of the SPIFFS frontend files (e.g. "/spiffs")
 * @param[in] fw_cbs     Firmware / mode callbacks — filled by AppController
 * @param[in] net_cbs    Network config callbacks — filled by NetworkStateMachine
 * @param[in] led_cbs    LED / night-mode callbacks — filled by StatusLightManager
 */
void esp_br_web_start(const char *base_path,
                      const web_firmware_callbacks_t *fw_cbs,
                      const web_network_callbacks_t  *net_cbs,
                      const web_led_callbacks_t      *led_cbs);

#ifdef __cplusplus
}
#endif
