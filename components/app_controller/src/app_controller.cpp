#include "app_controller.h"

#include "app_nvs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AppController";

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
                DeviceMode targetMode{AppNvs::readRcpUpdateTarget()};
                AppNvs::writeDeviceMode(targetMode);
                AppNvs::writeRcpPending(false);
                ESP_LOGI(TAG, "RCP flash done, mode set to %d — rebooting",
                         static_cast<int>(targetMode));
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

esp_err_t AppController::requestRcpFlash(const char* url, DeviceMode mode)
{
    ESP_RETURN_ON_ERROR(AppNvs::writeDeviceSetup(true),       TAG, "Write device setup failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpUrl(url),             TAG, "Write RCP URL failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpUpdateTarget(mode),   TAG, "Write RCP update target failed");
    ESP_RETURN_ON_ERROR(AppNvs::writeRcpPending(true),        TAG, "Write RCP pending failed");
    ESP_LOGI(TAG, "RCP flash scheduled (mode %d) — rebooting", static_cast<int>(mode));
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

    // Live switch only for coordinator-to-coordinator transitions (e.g. USB <-> Net).
    // Any transition involving Thread always goes through flash_rcp + reboot.
    if (firmwareManager.getActiveMode() != DeviceMode::THREAD && mode != DeviceMode::THREAD)
    {
        firmwareManager.stop();
        vTaskDelay(pdMS_TO_TICKS(50));
        firmwareManager.start(mode);
    }

    return ESP_OK;
}

DeviceMode AppController::getCurrentMode()
{
    if (AppNvs::readRcpPending())
        return AppNvs::readRcpUpdateTarget();
    return firmwareManager.getActiveMode();
}

void AppController::fillFirmwareCallbacks(web_firmware_callbacks_t* cbs)
{
    cbs->flash_rcp = [](void* ctx, const char* url, int mode)
    {
        return static_cast<AppController*>(ctx)->requestRcpFlash(url, static_cast<DeviceMode>(mode));
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
