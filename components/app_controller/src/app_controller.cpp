#include "app_controller.h"

#include "app_nvs.h"
#include "esp_check.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char* TAG = "AppController";

AppController::AppController(UpdateManager& updateMgr, FirmwareManager& firmwareMgr)
    : updateManager{updateMgr}, firmwareManager{firmwareMgr}
{}

void AppController::run()
{
    // First boot — wait for the user to select a mode via the web UI.
    // requestRcpFlash() completes setup live (flash + start the firmware
    // manager) without ever returning here — this loop just parks the task.
    if (!AppNvs::readDeviceSetup())
    {
        ESP_LOGI(TAG, "First boot — waiting for mode selection via web UI");
        while (true)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }

    // Normal boot — start the configured protocol stack
    DeviceMode mode{AppNvs::readDeviceMode()};
    ESP_LOGI(TAG, "Starting firmware manager in mode %d", static_cast<int>(mode));
    ESP_ERROR_CHECK(firmwareManager.start(mode));
}

esp_err_t AppController::requestRcpFlash(const char* url, DeviceMode mode)
{
    if (AppNvs::readDeviceSetup())
    {
        firmwareManager.stop();
    }

    esp_err_t err{updateManager.flashRcp(url)};
    if (err == ESP_OK)
    {
        err = updateManager.waitForRcpFlash();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "RCP flash failed: %s", esp_err_to_name(err));
        return err;
    }

    AppNvs::writeDeviceMode(mode);
    AppNvs::writeDeviceSetup(true);
    ESP_LOGI(TAG, "RCP flash done — starting firmware manager in mode %d", static_cast<int>(mode));

    return firmwareManager.start(mode);
}

esp_err_t AppController::requestEspFlash(const char* url)
{
    return updateManager.flashEsp(url);
}

esp_err_t AppController::requestModeChange(DeviceMode mode)
{
    ESP_RETURN_ON_ERROR(AppNvs::writeDeviceMode(mode), TAG, "Write device mode failed");
    DeviceMode currentMode {firmwareManager.getActiveMode()};
    ESP_LOGI(TAG, "Mode change from %d to %d", static_cast<int>(currentMode), static_cast<int>(mode));

    // Live switch only for coordinator-to-coordinator transitions (e.g. USB <-> Net).
    // Any transition involving Thread needs a fresh RCP flash — use requestRcpFlash().
    if (currentMode != DeviceMode::THREAD && mode != DeviceMode::THREAD)
    {
        esp_err_t ret = firmwareManager.stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Error shutting down Transport interface");
            return ESP_FAIL;
        }
            
        vTaskDelay(pdMS_TO_TICKS(50));
        ret = firmwareManager.start(mode);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Error starting new Transport interface");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

DeviceMode AppController::getCurrentMode()
{
    return firmwareManager.getActiveMode();
}

void AppController::espReboot()
{
    firmwareManager.stop();
    esp_restart();
}

esp_err_t AppController::espEraseNvs()
{
    return nvs_flash_erase();
}

esp_err_t AppController::rcpReboot()
{
    return firmwareManager.resetRcp();
}

esp_err_t AppController::rcpEraseNvram()
{
    return firmwareManager.factoryReset();
}

void AppController::setLogLevel(esp_log_level_t logLevel)
{
    if (logLevel)
        esp_log_level_set("*", logLevel);
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
    cbs->esp_reboot = [](void* ctx)
    {
        return static_cast<AppController*>(ctx)->espReboot();
    };
    cbs->esp_erase_nvs = [](void* ctx)
    {
        return static_cast<AppController*>(ctx)->espEraseNvs();
    };
    cbs->rcp_reboot = [](void* ctx)
    {
        return static_cast<AppController*>(ctx)->rcpReboot();
    };
    cbs->rcp_erase_nvram = [](void* ctx)
    {
        return static_cast<AppController*>(ctx)->rcpEraseNvram();
    };
    cbs->esp_set_log_level = [](void* ctx, int log_level)
    {
        return static_cast<AppController*>(ctx)->setLogLevel(static_cast<esp_log_level_t>(log_level));
    };
    cbs->ctx = this;
}
