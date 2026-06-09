#pragma once
#include "esp_err.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_SYS_NAMESPACE "sys_cfg"
#define NVS_RCP_URL_MAX_LEN 256

#define NVS_KEY_RCP_URL "rcp_url"
#define NVS_KEY_RCP_PENDING "rcp_pending"

#define NVS_KEY_DEVICE_SETUP "device_setup"

struct RcpFlashConfig {
    char url[NVS_RCP_URL_MAX_LEN];
    bool pendingFlash;
};

namespace SysNvsBinding {
    esp_err_t readRcpFlashConfig(RcpFlashConfig& config);
    esp_err_t writeRcpFlashConfig(const RcpFlashConfig& config);
    esp_err_t clearRcpFlashConfig();
    bool rcpFlashPending();

    /**
     * @brief   Returns Value Stored in NVS if the device was already started and RCP correctly flashed
     * 
     * @returns  bool - DeviceStatus
     */
    bool deviceSetup();

    /**
     * @brief   Write Device Setup value in NVS
     * 
     * @param[in]   isDeviceSetup - bool Device Setup Status
     * 
     * @returns  esp_err_t ESP_OK, ESP_FAIL
     */
    esp_err_t writeNvsDeviceSetup(bool isDeviceSetup);
}
