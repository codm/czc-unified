#include "app_controller.h"

#include "app_nvs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AppController";

constexpr const char* FIRST_BOOT_RCP_URL =
    "https://github.com/codm/czc-ot-rcp-fw/releases/download/V1.0.0/czc_ot_rcp_fw_1.0.0.bin";

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

    // 2. First boot — RCP not yet provisioned
    if (!AppNvs::readDeviceSetup())
    {
        ESP_LOGI(TAG, "First boot — scheduling initial RCP flash");
        AppNvs::writeRcpUrl(FIRST_BOOT_RCP_URL);
        AppNvs::writeRcpPending(true);
        AppNvs::writeDeviceSetup(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }

    // 3. Normal boot — start the configured protocol stack
    DeviceMode mode{AppNvs::readDeviceMode()};
    ESP_LOGI(TAG, "Starting firmware manager in mode %d", static_cast<int>(mode));
    ESP_ERROR_CHECK(firmwareManager.start(mode));
}

esp_err_t AppController::requestRcpFlash(const char* url)
{
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpUrl(url),      TAG, "Write RCP URL failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpPending(true), TAG, "Write RCP pending failed");
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
    ESP_LOGI(TAG, "Mode change to %d — rebooting", static_cast<int>(mode));
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

DeviceMode AppController::getCurrentMode()
{
    return firmwareManager.getActiveMode();
}

void AppController::fillWebCallbacks(system_flash_callbacks_t* cbs)
{
    cbs->flash_rcp = [](void* ctx, const char* url)
    {
        return static_cast<AppController*>(ctx)->requestRcpFlash(url);
    };
    cbs->flash_esp = [](void* ctx, const char* url)
    {
        return static_cast<AppController*>(ctx)->requestEspFlash(url);
    };
    cbs->ctx = this;
}
