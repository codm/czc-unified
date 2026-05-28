/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_err_t (*flash_esp)(void *ctx, const char *url);
    esp_err_t (*flash_rcp)(void *ctx, const char *url);
    void *ctx;
} system_flash_callbacks_t;

/**
 * @brief Start border router web server, which provides REST APIs and GUI
 *
 * @param[in] base_path    Virtual file path of web server
 * @param[in] flash_cbs    Callbacks for ESP/RCP firmware flash endpoints
 */
void esp_br_web_start(char *base_path, const system_flash_callbacks_t *flash_cbs);

#ifdef __cplusplus
}
#endif
