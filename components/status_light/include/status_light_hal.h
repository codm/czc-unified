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
};

#endif // CZC_STATUS_LIGHT_HAL_H_
