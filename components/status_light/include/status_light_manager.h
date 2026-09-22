#ifndef CZC_STATUS_LIGHT_MANAGER_H_
#define CZC_STATUS_LIGHT_MANAGER_H_

#include <array>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "status_light_callbacks.h"
#include "status_light_event.h"
#include "status_light_hal.h"

enum class LedId : uint8_t { PWR, MODE, RCP };
enum class LedOverride : uint8_t { AUTO, ON, OFF };
constexpr size_t LED_COUNT = 3;

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

    std::array<LedOverride, LED_COUNT> overrides;

    int night_mode_start_handle;
    int night_mode_end_handle;
    night_mode_config_t night_mode_config;

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
    static void eventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);

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

    /**
     * @brief Applies the override of the given LED on top of its automatic level.
     *
     * @param[in] led        LED whose override to apply
     * @param[in] autoLevel  Level computed by mapLedsToState()
     *
     * @return `autoLevel` for `AUTO`, fixed level for `ON` / `OFF`
     */
    bool resolveOverride(LedId led, bool autoLevel) const;

    /** 
     * @brief Cron job function - sets all LEDs to OFF
     * 
     * @return `ESP_OK`
     */
    static esp_err_t allLedsOff(void* ctx);

    /** 
     * @brief Cron job function - sets all LEDs to AUTO 
     * 
     * @return `ESP_OK`
     */
    static esp_err_t allLedsAuto(void* ctx);

public:
    /**
     * @brief Constructor — initialises all members to safe defaults.
     */
    StatusLightManager();

    /**
     * @brief Initialise the HAL, register the event handler and start the tick timer.
     *
     * @return `ESP_OK` on success, first failing esp_err_t otherwise
     */
    esp_err_t init();

    /**
     * @brief Force the given LED on / off, or hand it back to the automatic mapping.
     *
     *        PWR and MODE are applied on the next 100 ms tick. 
     *        RCP LED is written here once on actual change as it needs to shut down
     *        the protocol stack.
     *
     * @param[in] led   LED to override
     * @param[in] mode  `AUTO` follow `LedState` - `ON` / `OFF` fixed state
     *
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` unknown LED
     */
    esp_err_t forceLed(LedId led, LedOverride mode);

    /**
     * @brief Get the currently active override of the given LED.
     *
     * @param[in] led  LED to query
     *
     * @return `0` AUTO - `1` ON - `2` OFF (matches `LedOverride` and the web frontend),
     *         `0` for an unknown LED
     */
    int getOverride(LedId led) const;

    /**
     * @brief Registers cron job for nightmode
     * 
     * @param[in] config start and end time of nightmode 
     * 
     * @return `ESP_OK` on successful register - `ESP_FAIL` on error within scheduling jobs
     */
    esp_err_t registerNightMode(night_mode_config_t config);

    /** 
     * @brief Get currently active nightmode config
     * 
     * @return `ESP_OK` on success
     */
    esp_err_t getNightMode(night_mode_config_t& config);

    /** 
     * @brief deletes nightmode cron jobs and resets internal values
     * 
     * @return `ESP_OK` on successful remove - `ESP_ERR_INVALID_ARG` if handles are not found -
     *         `ESP_FAIL` handles are not set e.g -1
     */
    esp_err_t clearNightMode();

    /**
     * @brief Fill a `web_led_callbacks_t` struct with static C shims
     *        that forward LED override requests to this StatusLightManager instance.
     *
     * @param[out] cbs  LED callback struct to fill
     *
     * @return void
     */
    void fillLedCallbacks(web_led_callbacks_t* cbs);
};

#endif // CZC_STATUS_LIGHT_MANAGER_H_
