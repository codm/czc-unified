#ifndef CZC_THREAD_CONTROLLER_H_
#define CZC_THREAD_CONTROLLER_H_

#include "esp_err.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"
#include "esp_spiffs.h"
#include "esp_br_web.h"

class Thread_controller
{
private:
    static const char* TAG;
    static void rcp_failure_hardware_reset_handler();
    bool thread_active = false;

    esp_vfs_eventfd_config_t eventfd_config;
    esp_vfs_spiffs_conf_t web_server_conf;
    esp_openthread_config_t config;

public:
    esp_err_t init(const system_flash_callbacks_t *flash_cbs);
    esp_err_t start();
    esp_err_t stop();
    bool is_running();
};

#endif // CZC_THREAD_CONTROLLER_H_
