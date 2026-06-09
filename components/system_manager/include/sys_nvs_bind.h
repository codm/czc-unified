#pragma once
#include "esp_err.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_SYS_NAMESPACE "sys_cfg"
#define NVS_RCP_URL_MAX_LEN 256

#define NVS_KEY_RCP_URL "rcp_url"
#define NVS_KEY_RCP_PENDING "rcp_pending"

struct RcpFlashConfig {
    char url[NVS_RCP_URL_MAX_LEN];
    bool pendingFlash;
};

namespace SysNvsBinding {
    esp_err_t readRcpFlashConfig(RcpFlashConfig& config);
    esp_err_t writeRcpFlashConfig(const RcpFlashConfig& config);
    esp_err_t clearRcpFlashConfig();
    bool rcpFlashPending();
}
