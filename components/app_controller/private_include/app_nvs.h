#ifndef CZC_APP_NVS_H_
#define CZC_APP_NVS_H_

#include "esp_err.h"
#include "firmware_manager.h"

/**
 * @brief NVS bindings for AppController boot state.
 *
 *        All keys live in the "sys_cfg" namespace.
 *        Every function opens and closes its own handle.
 */
namespace AppNvs {

/**
 * @brief Read whether the device has been provisioned (RCP flashed at least once).
 *
 * @return true if device_setup flag is set in NVS, false otherwise
 */
bool readDeviceSetup();

/**
 * @brief Write the device_setup flag to NVS.
 *
 * @param[in] value  true = provisioned, false = not yet provisioned
 *
 * @return `ESP_OK` on success
 */
esp_err_t writeDeviceSetup(bool value);

/**
 * @brief Read whether an RCP flash has been scheduled.
 *
 * @return true if rcp_pending flag is set, false otherwise
 */
bool readRcpPending();

/**
 * @brief Write the rcp_pending flag to NVS.
 *
 * @param[in] pending  true = flash pending on next boot
 *
 * @return `ESP_OK` on success
 */
esp_err_t writeRcpPending(bool pending);

/**
 * @brief Read the pending RCP firmware URL from NVS.
 *
 * @param[out] outBuf  Buffer to receive the URL string
 * @param[in]  bufLen  Size of outBuf
 *
 * @return `ESP_OK` on success, `ESP_ERR_NVS_NOT_FOUND` if no URL stored
 */
esp_err_t readRcpUrl(char* outBuf, size_t bufLen);

/**
 * @brief Write the RCP firmware URL to NVS.
 *
 * @param[in] url  HTTPS URL of the TI firmware binary
 *
 * @return `ESP_OK` on success
 */
esp_err_t writeRcpUrl(const char* url);

/**
 * @brief Read the stored device mode from NVS.
 *
 * @return Stored `DeviceMode`, defaults to `DeviceMode::THREAD` if not set
 */
DeviceMode readDeviceMode();

/**
 * @brief Write the device mode to NVS.
 *
 * @param[in] mode  The mode to persist
 *
 * @return `ESP_OK` on success
 */
esp_err_t writeDeviceMode(DeviceMode mode);

} // namespace AppNvs

#endif // CZC_APP_NVS_H_
