#pragma once
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_crt_bundle.h"

#define EXAMPLE_OTA_RECV_TIMEOUT_MS 1000
#define EXAMPLE_OTA_BUF_SIZE 2048
#define OTA_TASK_STACK_SIZE 8192
#define OTA_TASK_PRIORITY 5

// Event Group Bits
#define OTA_SUCCESS_BIT BIT0
#define OTA_FAIL_BIT BIT1

class System_manager
{
private:
    static const char* TAG;
    const char* esp_download_url = "https://github.com/codm/czc-fw/releases/download/V2.1.0/czc_fw_2.1.0.ota.bin";

    // Event group to signal OTA result back to flashEspFirmware()
    EventGroupHandle_t ota_event_group = nullptr;

    static void ota_update_task(void* pvParameter);

public:
    System_manager();
    ~System_manager();
    void init();
    void close();
    void flashEspFirmware();
    void flashRcpFirmware();
};

// https://raw.githubusercontent.com/codm/CZC/refs/heads/zb_fws/ti/manifest.json
