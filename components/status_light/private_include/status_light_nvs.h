#ifndef STATUS_LIGHT_NVS_H_
#define STATUS_LIGHT_NVS_H_

#include <cstddef>
#include "esp_err.h"

namespace StatusLightNvs
{
    // read write night mode 
    // read write led state 

    /**
     * @brief Read the persisted Led overrides
     * 
     * @param[out] ovwrts Destination buffer for the stored overrides
     * @param[in,out] length In: size of `ovwrts` in bytes - Out: length of read data 
     * 
     * @return `ESP_OK` on success - `ESP_ERR_NVS_NOT_FOUND` if no values was stored yet -
     *         `ESP_ERR_NVS_INVALID_LENGTH` if ovwrts is too small
     */
    esp_err_t readOverrides(void* ovwrts, size_t& length);

    /**
     * @brief Save Led overrides to persistent storage
     * 
     * @param[in] ovwrts pointer to Array of Values to store
     * @param[in] length number of Values to store (size of array)
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on nullptr
    */
    esp_err_t writeOverrides(const void* ovwrts, size_t& length);

    /**
     * @brief Read the persisted Night mode settings
     * 
     * @param[out] nightModeState Destination buffer for the stored night mode state
     * @param[in,out] length In: size of `nightModeState` in bytes - Out: length of read data 
     * 
     * @return `ESP_OK` on success - `ESP_ERR_NVS_NOT_FOUND` if no values was stored yet -
     *         `ESP_ERR_NVS_INVALID_LENGTH` if nightModeState is too small
     */
    esp_err_t readNightModeState(void* nightModeState, size_t& length);

    /** 
     * @brief Save Night mode settings to persistent storage 
     * 
     * @param[in] nightModeState pointer to value to store
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on nullptr
     */
    esp_err_t writeNightModeState(const void* nightModeState, size_t& length);

    /**
     * @brief deletes NightModeState nvs key
     * 
     * @return Fail specific error message passed through from failed function 
     */
    esp_err_t clearNightModeState();
}

#endif /* STATUS_LIGHT_NVS_H_ */