#ifndef CZC_STATUS_LIGHT_MANAGER_H_
#define CZC_STATUS_LIGHT_MANAGER_H_

#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "status_light_event.h"
#include "status_light_hal.h"

/**
 * @brief Event-driven LED state manager for the two status LEDs.
 *
 *        Other components post `LedState` events into the default event loop
 *        via `esp_event_post(STATUS_LED_EVENT, ...)`. This manager subscribes,
 *        applies priority arbitration, and drives the LEDs through StatusLightHal.
 *
 *        A 100 ms esp_timer tick drives all blink patterns:
 *          - 1 Hz  : (tickCount % 10) < 5   → 500 ms on / 500 ms off
 *          - ~3 Hz : (tickCount % 3)  == 0  → 100 ms on / 200 ms off
 *
 * @note  Priority (highest first):
 *        ERROR > FLASHING > ZIGBEE_ERROR > ZIGBEE_CONNECTING > BOOTING >
 *        NETWORK_DOWN > THREAD_ACTIVE > ZIGBEE_USB > ZIGBEE_HOST_WAIT > ZIGBEE_NET
 */
class StatusLightManager {
private:
    static const char* TAG;

    StatusLightHal     ledHal;
    esp_timer_handle_t tickTimer;
    LedState           currentState;
    uint32_t           tickCount;
    bool ledOutputEnabled{true};

    /**
     * @brief ESP event handler — updates `currentState` if the incoming state has
     *        equal or higher priority than the active one.
     *
     * @param[in] arg        Pointer to the owning `StatusLightManager` instance
     * @param[in] event_base Must be `STATUS_LED_EVENT`
     * @param[in] event_id   `LedState` cast to `int32_t`
     * @param[in] event_data Unused
     *
     * @return void
     */
    static void ledEventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);

    /**
     * @brief ESP event handler for button events — toggles LED output or blinks both LEDs.
     *     * @param[in] arg        Pointer to the owning `StatusLightManager` instance
     * @param[in] event_base Must be `BUTTON_EVENTS`
     * @param[in] event_id   `BUTTON_EVENT_TOGGLE_LED` or `BUTTON_EVENT_BLINK_LED`
     * @param[in] event_data Unused
     * @return void
     */
    static void buttonEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    /**
     * @brief esp_timer callback — increments the tick counter and triggers mapLedsToState().
     *
     * @param[in] arg  Pointer to the owning `StatusLightManager` instance
     *
     * @return void
     */
    static void timerCallback(void* arg);

    /**
     * @brief Maps currentState and tickCount to concrete LED on/off levels.
     *
     *        Called every 100 ms from timerCallback(). Must not block.
     *
     * @return void
     */
    void mapLedsToState();

public:
    /**
     * @brief Constructor — initialises all members to safe defaults.
     */
    StatusLightManager();

    /**
     * @brief Enable or disable LED output.
     *
     *        When disabled, both LEDs are forced off regardless of currentState.
     *
     * @param[in] enabled  true = enable LED output, false = force both LEDs off
     *
     * @return void
     */
    void setLedOutputEnabled(bool enabled);


    /**
     * @brief Check if LED output is enabled.
     *
     * @return true if LED output is enabled, false if both LEDs are forced off
     */
    bool isLedOutputEnabled() const;

    /**
     * @brief Blink both LEDs for a specified duration.
     *
     *        This is a blocking call that overrides the currentState and tickCount
     *        for the duration of the blink. After the duration, the previous state
     *        is restored.
     *
     * @param[in] durationMs  Duration in milliseconds to blink both LEDs
     *
     * @return void
     */
    void blinkLeds(int durationMs);

    /**
     * @brief Initialise the HAL, register the event handler and start the tick timer.
     *
     * @return `ESP_OK` on success, first failing esp_err_t otherwise
     */
    esp_err_t init();
};

#endif // CZC_STATUS_LIGHT_MANAGER_H_
