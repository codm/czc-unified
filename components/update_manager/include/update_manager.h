#ifndef CZC_UPDATE_MANAGER_H_
#define CZC_UPDATE_MANAGER_H_

#include "esp_err.h"

/**
 * @brief Manages RCP and ESP firmware updates with OTA-partition mutual exclusion.
 *
 *        Owns a RcpUpdater (TI binary, boot-time only) and an OtaUpdater
 *        (ESP app image, live). Both use the same OTA partition — a `busy` flag
 *        prevents them from running simultaneously.
 *
 *        Intended usage:
 *          - flashRcp() is called at boot time by AppController when
 *            `rcp_flash_pending` is set in NVS.
 *          - flashEsp() is called live from the web API callback.
 */
class UpdateManager {
public:
    /**
     * @brief Constructor — initialises members to safe defaults.
     */
    UpdateManager();

    /**
     * @brief Download and flash RCP firmware from `url` via the BSL protocol.
     *
     *        Blocking — runs the full download→erase→program→reset sequence.
     *        Must only be called at boot time before any other UART user starts.
     *
     * @param[in] url  HTTPS URL of the TI firmware binary (.bin)
     *
     * @return `ESP_OK` on success,
     *         `ESP_ERR_INVALID_STATE` if another update is already in progress
     */
    esp_err_t flashRcp(const char* url);

    /**
     * @brief Start an OTA update for the ESP firmware from `url`.
     *
     *        Non-blocking — spawns an internal task. The task reboots the device
     *        on success.
     *
     * @param[in] url  HTTPS URL of the ESP firmware image (.bin)
     *
     * @return `ESP_OK` if the update task was started,
     *         `ESP_ERR_INVALID_STATE` if another update is already in progress
     */
    esp_err_t flashEsp(const char* url);

private:
    bool busy;
};

#endif // CZC_UPDATE_MANAGER_H_
