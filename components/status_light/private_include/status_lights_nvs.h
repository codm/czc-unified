#ifndef STATUS_LIGHTS_NVS_H_
#define STATUS_LIGHTS_NVS_H_

#include "esp_err.h"

namespace StatusLightsNvs
{
    // read write night mode 
    // read write led state 

    /**
     * @brief Read the persisted Led Overwrites
     * 
     * @param[out] ovwrts Destination buffer for the stored overwrites
     * @param[in,out] length In: size of `ovwrts` in bytes - Out: length of read data 
     * 
     * @return `ESP_OK` on success - `ESP_ERR_NVS_NOT_FOUND` if no values was stored yet -
     *         `ESP_ERR_NVS_INVALID_LENGTH` if ovwrts is too small
     */
    esp_err_t readOverwrites(uint8_t* ovwrts, size_t& length);

    /**
     * @brief Save Led Overwrites to persistent storage
     * 
     * @param[in] ovwrts pointer to Array of Values to store
     * @param[in] length number of Values to store (size of array)
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on nullptr
    */
    esp_err_t writeOverwrites(const uint8_t* ovwrts, uint8_t length);

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
    esp_err_t writeNightModeState(void* nightModeState);
}

#endif /* STATUS_LIGHTS_NVS_H_ */