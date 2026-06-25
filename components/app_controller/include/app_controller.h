#ifndef CZC_APP_CONTROLLER_H_
#define CZC_APP_CONTROLLER_H_

#include "esp_err.h"
#include "firmware_callbacks.h"
#include "update_manager.h"
#include "firmware_manager.h"

/**
 * @brief Composition root and boot orchestrator.
 *
 *        Reads NVS intent flags at boot, decides whether to flash, provision
 *        or start the active protocol mode, and routes web API requests to
 *        the appropriate sub-manager.
 *
 *        Boot-Decision-Tree:
 *          1. rcp_flash_pending → flashRcp → store chip info → clear flag → reboot
 *          2. !device_setup    → wait until web interface schedules reboot via device setup pop up
 *          3. otherwise        → firmware_manager.start(stored mode)
 *
 * @note  UpdateManager and FirmwareManager are injected by reference from main
 *        so AppController never owns them.
 */
class AppController {
public:
    /**
     * @brief Constructor — stores references to the injected managers.
     *
     * @param[in] updateMgr    Manages RCP and ESP firmware updates
     * @param[in] firmwareMgr  Selects and drives the active protocol stack
     */
    AppController(UpdateManager& updateMgr, FirmwareManager& firmwareMgr);

    /**
     * @brief Execute the boot-decision-tree, then enter normal operation.
     *
     *        Blocking — this function only returns when the firmware manager
     *        starts the protocol stack. Pending flash paths reboot the device
     *        and never return.
     *
     * @return void
     */
    void run();

    /**
     * @brief Fill a `web_firmware_callbacks_t` struct with static C shims
     *        that forward firmware / mode requests to this AppController instance.
     *
     *        Call this before `esp_br_web_start()` in main.
     *
     * @param[out] cbs  Firmware callback struct to fill
     *
     * @return void
     */
    void fillFirmwareCallbacks(web_firmware_callbacks_t* cbs);

    /**
     * @brief Schedule an RCP firmware update: write intent to NVS and reboot.
     *
     *        The actual flashing happens on the next boot via the boot-decision-tree.
     *
     * @param[in] url  HTTPS URL of the TI firmware binary
     *
     * @return `ESP_OK` if the intent was written successfully
     */
    esp_err_t requestRcpFlash(const char* url);

    /**
     * @brief Start a live ESP OTA update.
     *
     * @param[in] url  HTTPS URL of the ESP firmware image
     *
     * @return `ESP_OK` if the update task was started
     */
    esp_err_t requestEspFlash(const char* url);

    /**
     * @brief Switch device mode: write new mode
     *
     * @param[in] mode  The target DeviceMode
     *
     * @return `ESP_OK` if the intent was written successfully
     */
    esp_err_t requestModeChange(DeviceMode mode);

    /**
     * @brief Return the currently active device mode.
     *
     * @return `DeviceMode` as reported by the FirmwareManager
     */
    DeviceMode getCurrentMode();

private:
    UpdateManager&  updateManager;
    FirmwareManager& firmwareManager;
};

#endif // CZC_APP_CONTROLLER_H_
