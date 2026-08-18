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
     * @brief Flash RCP firmware live and activate `mode` — no reboot.
     *
     *        Stops the currently active protocol controller (if any) to free the
     *        RCP UART, flashes via UpdateManager, then writes device_mode and
     *        device_setup to NVS and starts the firmware manager in `mode`.
     *        Blocks the calling task until the flash finishes.
     *
     * @param[in] url   HTTPS URL of the TI firmware binary
     * @param[in] mode  DeviceMode to activate once the flash completes
     *
     * @return `ESP_OK` on success
     */
    esp_err_t requestRcpFlash(const char* url, DeviceMode mode);
    
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
    
    /**
     * @brief Shuts down interface and reboots ESP
     * 
     * @return void
     */
    void espReboot();
    
    /**
     * @brief Erase ESP non volatile storage
     * 
     * @return `ESP_OK` on success - 
     * 
     *          ESP_ERR_NOT_FOUND if there is no NVS partition labeled "nvs" in the partition table - 
     * 
     *          different error in case de-initialization fails (shouldn't happen) 
     */
    esp_err_t espEraseNvs();

    /**
     * @brief Hardware-resets the RCP, using whichever mechanism the
     *        currently active protocol (Zigbee/Thread) implements.
     *
     *        Zigbee: reboots via `ZstackMt` (Z-Stack MT protocol).
     *        Thread: pulses the RST GPIO directly — the Spinel driver
     *        resynchronises on its own, no stack restart needed.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t rcpReboot();

    /**
     * @brief Erases the RCP's persisted network config, using whichever
     *        mechanism the currently active protocol (Zigbee/Thread) implements.
     *
     *        Zigbee: erases Z-Stack NVRAM (network table & config) via `ZstackMt`.
     *        Thread: erases the Thread dataset/settings via the OpenThread
     *        settings API — the RCP itself holds no meaningful config in
     *        Thread mode.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t rcpEraseNvram();
    
    /**
     * @brief 
     */
    void setLogLevel();

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
    
private:
    UpdateManager&  updateManager;
    FirmwareManager& firmwareManager;
};

#endif // CZC_APP_CONTROLLER_H_
