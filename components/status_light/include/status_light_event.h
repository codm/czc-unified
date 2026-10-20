#ifndef CZC_STATUS_LIGHT_EVENT_H_
#define CZC_STATUS_LIGHT_EVENT_H_

#include <cstddef>

#include "esp_event.h"
#include "esp_log.h"

ESP_EVENT_DECLARE_BASE(STATUS_LED_EVENT);

/** @brief Posted to change and react to changes of the RCP LED in the firmware manager */
ESP_EVENT_DECLARE_BASE(RCP_LED_EVENT);

/**
 * @brief LED states - higher value = higher display priority.
 *
 *        All values up to and including `BOOTING` are operating modes (only one active).
 *
 * Usage:
 *   StatusLedMode::set(LedState::THREAD_ACTIVE);
 *   StatusLedModeScope flashing{LedState::FLASHING};
 */
enum class LedState : int32_t {
    ZIGBEE_NET,         ///< Zigbee mode, network host active       — pwr on, mode LED off
    ZIGBEE_USB,         ///< Zigbee mode, USB host active           — pwr on, mode LED on
    THREAD_ACTIVE,      ///< Thread/OTBR running                    — pwr on, mode blink 1x/s
    ZIGBEE_HOST_WAIT,   ///< Zigbee mode, no host connected yet     — pwr blinks 1x/s, mode LED off
    ZIGBEE_ROUTER,      ///< Zigbee in Router mode                  - pwr on, mode LED off
    BOOTING,            ///< System startup                         — pwr on, mode LED off 
    NETWORK_DOWN,       ///< No network connection                  — pwr blinks 3x/s, mode LED off
    ZIGBEE_ERROR,       ///< ZigBee communication error             — pwr on, mode blinks 3x/s
    FLASHING,           ///< Firmware update in progress            — pwr & mode blink 1x/s
    ERROR,              ///< General fault                          — pwr & mode blink 3x/s
    ELEMENT_COUNT       // Size of the ENUM < DONT USE >
};
constexpr size_t OPERATING_MODES_COUNT {static_cast<size_t>(LedState::BOOTING) + 1}; // only one can be active at a time

inline static void sendLedChangeEvent(LedState state, bool value) {
    if (esp_event_post(STATUS_LED_EVENT, static_cast<int32_t>(state), &value, sizeof(value), 0) != ESP_OK)
        ESP_LOGW("StatusLedMode", "Dropped LED state %d (%s)", static_cast<int>(state), value ? "set" : "clear");
}

namespace StatusLedMode 
{
    inline void set(LedState state) {
        sendLedChangeEvent(state, true);
    };
    inline void clear(LedState state) {
        sendLedChangeEvent(state, false);
    };
}

/**
 * @brief   RAII wrapper for LED Set and clear
 *          
 *          If this object goes out of scope the LED state is cleared automatically in the destructor
 */
class StatusLedModeScope {
    public:
        explicit StatusLedModeScope(LedState s) : state{s} { StatusLedMode::set(state); }
        ~StatusLedModeScope()                              { StatusLedMode::clear(state); }
    private:
        LedState state;
};

#endif // CZC_STATUS_LIGHT_EVENT_H_
