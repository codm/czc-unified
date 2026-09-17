#ifndef CZC_STATUS_LIGHT_EVENT_H_
#define CZC_STATUS_LIGHT_EVENT_H_

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(STATUS_LED_EVENT);

/** @brief Posted with the arbitrated `LedState` whenever it changes. */
ESP_EVENT_DECLARE_BASE(RCP_LED_EVENT);

/**
 * @brief LED states posted as events into the default event loop.
 *
 * Higher numeric value = higher display priority.
 * The StatusLightManager ignores events with lower priority than the
 * currently active state.
 *
 * Usage:
 *   esp_event_post(STATUS_LED_EVENT, static_cast<int32_t>(LedState::FLASHING),
 *                  nullptr, 0, 0);
 */
enum class LedState : int32_t {
    ZIGBEE_NET        = 0,  ///< Zigbee mode, network host active       — pwr on, mode LED off
    ZIGBEE_USB        = 1,  ///< Zigbee mode, USB host active           — pwr on, mode LED on
    THREAD_ACTIVE     = 2,  ///< Thread/OTBR running                    — pwr on, mode blink 1x/s
    ZIGBEE_HOST_WAIT  = 3,  ///< Zigbee mode, no host connected yet     — pwr blinks 1x/s, mode LED off
    NETWORK_DOWN      = 4,  ///< No network connection                  — pwr blinks 3x/s, mode LED off
    BOOTING           = 5,  ///< System startup                         — pwr on, mode LED off 
    ZIGBEE_ERROR      = 6,  ///< ZigBee communication error             — pwr on, mode blinks 3x/s
    FLASHING          = 7,  ///< Firmware update in progress            — pwr & mode blink 1x/s
    ERROR             = 8,  ///< General fault                          — pwr & mode blink 3x/s
};

#endif // CZC_STATUS_LIGHT_EVENT_H_
