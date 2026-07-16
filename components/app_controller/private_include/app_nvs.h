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
