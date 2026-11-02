#ifndef CZC_STATUS_LIGHT_HAL_H_
#define CZC_STATUS_LIGHT_HAL_H_

#include "esp_err.h"

/**
 * @brief HAL for the two status LEDs (Power/green and Mode/red).
 *
 *        Pin assignments and active-HIGH polarity come from board_config.h.
 *        This class is private to the status_light component — callers interact
 *        only via StatusLightManager and the STATUS_LED_EVENT event loop.
 */
class StatusLightHal {
public:
    /**
     * @brief Configure GPIO outputs and turn both LEDs off.
     *
     * @return `ESP_OK` on success, propagated GPIO error otherwise
     */
    esp_err_t init();

    /**
     * @brief Set the green Power LED state.
     *
     * @param[in] enabled  true = lit, false = off
     *
     * @return void
     */
    void setPwr(bool enabled);

    /**
     * @brief Set the red Mode LED state.
     *
     * @param[in] enabled  true = lit, false = off
     *
     * @return void
     */
    void setMode(bool enabled);

    /**
     * @brief Set the yellow ZigBee LED state
     * 
     * @param[in] enabled true = lit, false = off
     * 
     * @warning Implemented using event - which intern gets processed by the firmware_manager component. 
     *          LED change only works whilst in ZigBee Mode. As a prerequisite for the change the proxy has to be 
     *          temporarily disabled. 
     * 
     * @note    Firmware manager uses a tiny queue layer to keep the event handler slim. Changes are pushed into this one slot queue
     *          and then processed by a task running every 100ms.
     * 
     * @returns `void`
     */
    void setRcp(bool enabled);
};

#endif // CZC_STATUS_LIGHT_HAL_H_
