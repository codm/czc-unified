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

#include "Rcp_interface.h"
#include "Thread_controller.h"
#include "sys_nvs_bind.h"

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

    uart_port_t rcp_uart_num;
    Rcp_interface rcp_interface;
    Thread_controller thread_controller;

    // Event group to signal OTA result back to flashEspFirmware()
    EventGroupHandle_t ota_event_group = nullptr;

    static void ota_update_task(void* pvParameter);

public:
    System_manager(Rcp_interface _rcp_interface, uart_port_t _rcp_uart_num);
    ~System_manager();
    esp_err_t initThread();
    esp_err_t startThread();
    void flashEspFirmware(const char* url);
    void flashRcpFirmware(const char* url);
    
    void initRcpFirmwareFlash(const char* url);
    void flashRcpFirmwareWhenConfigured();
    bool isRcpFlashPending();

    static void ota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};

struct EspFlashConfig {
    System_manager *self;
    char url[256];
};
// https://raw.githubusercontent.com/codm/CZC/refs/heads/zb_fws/ti/manifest.json
