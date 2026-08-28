#include "status_light_manager.h"

#include "esp_check.h"
#include "esp_log.h"

const char* StatusLightManager::TAG = "StatusLightManager";

ESP_EVENT_DEFINE_BASE(STATUS_LED_EVENT);
ESP_EVENT_DEFINE_BASE(RCP_LED_EVENT);

StatusLightManager::StatusLightManager()
    : ledHal{}, tickTimer{nullptr}, currentState{LedState::BOOTING}, tickCount{0}, lastRcpLed{false}
{}

esp_err_t StatusLightManager::init()
{
    ESP_RETURN_ON_ERROR(ledHal.init(), TAG, "HAL init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(STATUS_LED_EVENT, ESP_EVENT_ANY_ID,
                                   &eventHandler, this),
        TAG, "Event handler register failed");

    esp_timer_create_args_t timerArgs{};
    timerArgs.callback = &timerCallback;
    timerArgs.arg      = this;
    timerArgs.name     = "status_light_tick";

    ESP_RETURN_ON_ERROR(esp_timer_create(&timerArgs, &tickTimer),
                        TAG, "Timer create failed");

    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tickTimer, 100 * 1000 /* 100 ms */),
                        TAG, "Timer start failed");

    ESP_LOGI(TAG, "Initialised");
    return ESP_OK;
}

void StatusLightManager::eventHandler(void* arg, esp_event_base_t, int32_t event_id, void*)
{
    auto* self             = static_cast<StatusLightManager*>(arg);
    auto  incomingLedState = static_cast<LedState>(event_id);

    if (static_cast<int32_t>(incomingLedState) >= static_cast<int32_t>(self->currentState)) {
        self->currentState = incomingLedState;
    }
}

void StatusLightManager::timerCallback(void* arg)
{
    auto* self = static_cast<StatusLightManager*>(arg);
    self->tickCount++;
    self->mapLedsToState();
}

void StatusLightManager::mapLedsToState()
{
    // Blink helpers based on tickCount (one tick = 100 ms):
    //   blink1Hz : 500 ms on / 500 ms off  (period = 10 ticks)
    //   blink3Hz : 100 ms on / 200 ms off  (period =  3 ticks, ≈ 3.3 Hz)
    bool blink1Hz{(tickCount % 10) < 5};
    bool blink3Hz{(tickCount % 3) == 0};

    bool pwrLed{false};
    bool modeLed{false};
    bool rcpLed{false};

    switch (currentState) {
        case LedState::ZIGBEE_NET:
            pwrLed  = true;
            modeLed = false;
            rcpLed  = true;
            break;

        case LedState::ZIGBEE_USB:
            pwrLed  = true;
            modeLed = true;
            rcpLed  = true;
            break;

        case LedState::THREAD_ACTIVE:
            pwrLed  = true;
            modeLed = false;
            rcpLed  = false;
            break;

        case LedState::ZIGBEE_HOST_WAIT:
        case LedState::NETWORK_DOWN:
        case LedState::BOOTING:
            pwrLed  = blink1Hz;
            modeLed = false;
            rcpLed  = false;
            break;

        case LedState::ZIGBEE_CONNECTING:
            pwrLed  = true;
            modeLed = blink1Hz;
            rcpLed  = false;
            break;

        case LedState::ZIGBEE_ERROR:
            pwrLed  = true;
            modeLed = blink3Hz;
            rcpLed  = false;
            break;

        case LedState::FLASHING:
        case LedState::ERROR:
            pwrLed  = blink3Hz;
            modeLed = blink3Hz;
            rcpLed  = false;
            break;
    }

    ledHal.setPwr(pwrLed);
    ledHal.setMode(modeLed);

    if (rcpLed != lastRcpLed) {
        lastRcpLed = rcpLed;
        esp_event_post(RCP_LED_EVENT, rcpLed ? 1 : 0, nullptr, 0, 0);
    }
}
