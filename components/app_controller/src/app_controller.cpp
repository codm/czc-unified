#include "app_controller.h"

#include "app_nvs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AppController";

constexpr char* RCP_URL_THREAD  =
    "https://github.com/codm/czc-ot-rcp-fw/releases/download/V1.0.0/czc_ot_rcp_fw_1.0.0.bin";
constexpr char* RCP_URL_ZIGBEE_COORD =
    "https://raw.githubusercontent.com/codm/czc-fw/zb_fws/ti/coordinator/CC1352P7_coordinator_20250321.bin";
constexpr char* RCP_URL_ZIGBEE_ROUTER =
    "https://raw.githubusercontent.com/codm/czc-fw/zb_fws/ti/router/CC1352P7_router_20250403.bin";

static const char* rcpUrlForMode(DeviceMode mode)
{
    switch (mode)
    {
        case DeviceMode::THREAD:        return RCP_URL_THREAD;
        case DeviceMode::ZIGBEE_USB:    return RCP_URL_ZIGBEE_COORD;
        case DeviceMode::ZIGBEE_NET:    return RCP_URL_ZIGBEE_COORD;
        case DeviceMode::ZIGBEE_ROUTER: return RCP_URL_ZIGBEE_ROUTER;
    }
    return RCP_URL_THREAD;
}

AppController::AppController(UpdateManager& updateMgr, FirmwareManager& firmwareMgr)
    : updateManager{updateMgr}, firmwareManager{firmwareMgr}
{}

void AppController::run()
{
    // 1. Pending RCP flash from a prior web request or provisioning
    if (AppNvs::readRcpPending())
    {
        char rcpUrl[256]{};
        esp_err_t err{AppNvs::readRcpUrl(rcpUrl, sizeof(rcpUrl))};

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "rcp_pending set but URL unreadable (%s) — clearing",
                     esp_err_to_name(err));
            AppNvs::writeRcpPending(false);
        }
        else
        {
            ESP_LOGI(TAG, "RCP flash pending: %s", rcpUrl);
            err = updateManager.flashRcp(rcpUrl);

            if (err == ESP_OK)
            {
                AppNvs::writeRcpPending(false);
                ESP_LOGI(TAG, "RCP flash done — rebooting");
            }
            else
            {
                ESP_LOGE(TAG, "RCP flash failed (%s) — rebooting", esp_err_to_name(err));
            }

            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
    }

    // 2. First boot — wait for the user to select a mode via the web UI.
    //    requestModeChange() will write device_setup, RCP URL and pending flag,
    //    then reboot — this loop never exits on its own.
    if (!AppNvs::readDeviceSetup())
    {
        ESP_LOGI(TAG, "First boot — waiting for mode selection via web UI");
        while (true)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }

    // 3. Normal boot — start the configured protocol stack
    DeviceMode mode{AppNvs::readDeviceMode()};
    ESP_LOGI(TAG, "Starting firmware manager in mode %d", static_cast<int>(mode));
    ESP_ERROR_CHECK(firmwareManager.start(mode));
}

esp_err_t AppController::requestRcpFlash(const char* url)
{
    ESP_RETURN_ON_ERROR(AppNvs::writeDeviceSetup(true), TAG, "Write device setup failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpUrl(url),       TAG, "Write RCP URL failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpPending(true),  TAG, "Write RCP pending failed");
    ESP_LOGI(TAG, "RCP flash scheduled — rebooting");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

esp_err_t AppController::requestEspFlash(const char* url)
{
    return updateManager.flashEsp(url);
}

esp_err_t AppController::requestModeChange(DeviceMode mode)
{
    ESP_RETURN_ON_ERROR(AppNvs::writeDeviceMode(mode), TAG, "Write device mode failed");
    ESP_LOGI(TAG, "Mode change to %d", static_cast<int>(mode));

    // Live switch only for non-Thread modes (e.g. coordinator USB <-> Net).
    // Thread mode changes always go through flash_rcp which handles the reboot.
    if (firmwareManager.getActiveMode() != DeviceMode::THREAD)
    {
        firmwareManager.stop();
        vTaskDelay(pdMS_TO_TICKS(50));
        firmwareManager.start(mode);
    }

    return ESP_OK;
}

DeviceMode AppController::getCurrentMode()
{
    return firmwareManager.getActiveMode();
}

void AppController::fillFirmwareCallbacks(web_firmware_callbacks_t* cbs)
{
    cbs->flash_rcp = [](void* ctx, const char* url)
    {
        return static_cast<AppController*>(ctx)->requestRcpFlash(url);
    };
    cbs->flash_esp = [](void* ctx, const char* url)
    {
        return static_cast<AppController*>(ctx)->requestEspFlash(url);
    };
    cbs->set_mode = [](void* ctx, int mode)
    {
        return static_cast<AppController*>(ctx)->requestModeChange(static_cast<DeviceMode>(mode));
    };
    cbs->get_mode = [](void* ctx, int* mode_out)
    {
        *mode_out = static_cast<int>(static_cast<AppController*>(ctx)->getCurrentMode());
        return ESP_OK;
    };
    cbs->get_device_setup = [](void* ctx, int* setup_out)
    {
        *setup_out = AppNvs::readDeviceSetup() ? 1 : 0;
        return ESP_OK;
    };
    cbs->ctx = this;
}
