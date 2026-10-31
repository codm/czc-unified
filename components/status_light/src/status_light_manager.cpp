#include "status_light_manager.h"

#include "esp_check.h"
#include "esp_log.h"

const char* StatusLightManager::TAG = "StatusLightManager";

ESP_EVENT_DEFINE_BASE(STATUS_LED_EVENT);

StatusLightManager::StatusLightManager()
    : ledHal{}, tickTimer{nullptr}, currentState{LedState::BOOTING}, tickCount{0}
{}

esp_err_t StatusLightManager::init()
{
    ESP_RETURN_ON_ERROR(ledHal.init(), TAG, "HAL init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(STATUS_LED_EVENT, ESP_EVENT_ANY_ID,
                                   &ledEventHandler, this),
        TAG, "LED Event handler register failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(BUTTON_EVENTS, ESP_EVENT_ANY_ID,
                                   &buttonEventHandler, this),
        TAG, "Button Event handler register failed");

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

void StatusLightManager::ledEventHandler(void* arg, esp_event_base_t /*event_base*/,
                                      int32_t event_id, void* /*event_data*/)
{
    auto* self             = static_cast<StatusLightManager*>(arg);
    auto  incomingLedState = static_cast<LedState>(event_id);

    if (static_cast<int32_t>(incomingLedState) >= static_cast<int32_t>(self->currentState)) {
        self->currentState = incomingLedState;
    }
}

void StatusLightManager::buttonEventHandler(void* arg, esp_event_base_t /*event_base*/,
                                      int32_t event_id, void* /*event_data*/)
{
    auto* self             = static_cast<StatusLightManager*>(arg);

    if (event_id == BUTTON_EVENT_TOGGLE_LED) {
        self->setLedOutputEnabled(!self->isLedOutputEnabled());
    } else if (event_id == BUTTON_EVENT_BLINK_LED) {
        self->blinkLeds(1500);
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

    switch (currentState) {
        case LedState::ZIGBEE_NET:
            pwrLed  = true;
            modeLed = false;
            break;

        case LedState::ZIGBEE_USB:
            pwrLed  = true;
            modeLed = true;
            break;

        case LedState::THREAD_ACTIVE:
            pwrLed  = true;
            modeLed = false;
            break;

        case LedState::ZIGBEE_HOST_WAIT:
        case LedState::NETWORK_DOWN:
        case LedState::BOOTING:
            pwrLed  = blink1Hz;
            modeLed = false;
            break;

        case LedState::ZIGBEE_CONNECTING:
            pwrLed  = true;
            modeLed = blink1Hz;
            break;

        case LedState::ZIGBEE_ERROR:
            pwrLed  = true;
            modeLed = blink3Hz;
            break;

        case LedState::FLASHING:
        case LedState::ERROR:
            pwrLed  = blink3Hz;
            modeLed = blink3Hz;
            break;
    }

    if (ledOutputEnabled) {
        ledHal.setPwr(pwrLed);
        ledHal.setMode(modeLed);
    }

    
}




void StatusLightManager::setLedOutputEnabled(bool enabled)
{
    ledOutputEnabled = enabled;

    if (!enabled) {
        ledHal.setPwr(false);
        ledHal.setMode(false);
    }
}


bool StatusLightManager::isLedOutputEnabled() const
{
    return ledOutputEnabled;
}

void StatusLightManager::blinkLeds(int durationMs)
{
    ledOutputEnabled = false;
    ledHal.setPwr(true);
    ledHal.setMode(true);
    vTaskDelay(pdMS_TO_TICKS(1500));
    ledHal.setPwr(false);
    ledHal.setMode(false);
    vTaskDelay(pdMS_TO_TICKS(1500));
}