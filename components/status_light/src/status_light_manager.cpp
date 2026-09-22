#include "status_light_manager.h"

#include "esp_check.h"
#include "esp_log.h"
#include "cron.h"

const char* StatusLightManager::TAG = "StatusLightManager";

ESP_EVENT_DEFINE_BASE(STATUS_LED_EVENT);
ESP_EVENT_DEFINE_BASE(RCP_LED_EVENT);

StatusLightManager::StatusLightManager()
    : ledHal{}, tickTimer{nullptr}, currentState{LedState::BOOTING}, tickCount{0}, overrides{}
{
    overrides.fill(LedOverride::AUTO);
}

esp_err_t StatusLightManager::init()
{
    ESP_RETURN_ON_ERROR(ledHal.init(), TAG, "HAL init failed");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(STATUS_LED_EVENT, ESP_EVENT_ANY_ID,
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

esp_err_t StatusLightManager::forceLed(LedId led, LedOverride mode)
{
    auto index = static_cast<size_t>(led);
    ESP_RETURN_ON_FALSE(index < LED_COUNT, ESP_ERR_INVALID_ARG, TAG, "Unknown LED %u", index);
    ESP_RETURN_ON_FALSE(mode <= LedOverride::OFF, ESP_ERR_INVALID_ARG, TAG, "Unknown override %u",
                        static_cast<unsigned>(mode));

    if (overrides[index] == mode) {
        return ESP_OK;
    }
    overrides[index] = mode;

    if (led == LedId::RCP && mode != LedOverride::AUTO) {
        ledHal.setRcp(mode == LedOverride::ON);
    }

    return ESP_OK;
}

int StatusLightManager::getOverride(LedId led) const
{
    auto index = static_cast<size_t>(led);
    return index < LED_COUNT ? static_cast<int>(overrides[index]) : static_cast<int>(LedOverride::AUTO);
}

esp_err_t StatusLightManager::registerNightMode(night_mode_config_t config)
{
    clearNightMode();

    cron_timing_t night_mode_timing {
        .minute = config.start_minute,
        .hour = config.start_hour,
        .dotm = CRON_ANY,
        .month = CRON_ANY,
        .weekday = CRON_ANY
    };
    int ret = Cron::scheduleJob(night_mode_timing, allLedsOff, this);
    if (ret == -1) {
        ESP_LOGE(TAG, "Error scheduling nightmode start job");
        return ESP_FAIL;
    }
    night_mode_start_handle = ret;

    night_mode_timing.minute = config.end_minute;
    night_mode_timing.hour = config.end_hour;
    ret = Cron::scheduleJob(night_mode_timing, allLedsAuto, this);
    if (ret == -1) {
        ESP_LOGE(TAG, "Error scheduling nightmode end job");
        return ESP_FAIL;
    }
    night_mode_end_handle = ret;
    night_mode_config = config;

    return ESP_OK;
}

esp_err_t StatusLightManager::getNightMode(night_mode_config_t& config)
{
    config = night_mode_config;
    return ESP_OK;
}

esp_err_t StatusLightManager::clearNightMode()
{
    if (night_mode_start_handle == -1 || night_mode_end_handle == -1) {
        ESP_LOGW(TAG, "tried to clear NightMode - at least one handle is not set!");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(Cron::removeJob(night_mode_start_handle), TAG, "Could not remove nightmode start job");
    ESP_RETURN_ON_ERROR(Cron::removeJob(night_mode_end_handle), TAG, "Could not remove nightmode end job");
    return ESP_OK;
}

void StatusLightManager::fillLedCallbacks(web_led_callbacks_t *cbs)
{
    cbs->set_override = [](void* ctx, int led, int mode)
    {
        return static_cast<StatusLightManager*>(ctx)->forceLed(static_cast<LedId>(led),
                                                              static_cast<LedOverride>(mode));
    };
    cbs->get_override = [](void* ctx, int led, int* mode_out)
    {
        *mode_out = static_cast<StatusLightManager*>(ctx)->getOverride(static_cast<LedId>(led));
        return ESP_OK;
    };
    cbs->set_night_mode = [](void* ctx, const night_mode_config_t* cfg) {
        return static_cast<StatusLightManager*>(ctx)->registerNightMode(*cfg);
    };
    cbs->get_night_mode = [](void* ctx, night_mode_config_t* cfg) {
        return static_cast<StatusLightManager*>(ctx)->getNightMode(*cfg);
    };
    cbs->set_clear_night_mode = [](void* ctx) {
        return static_cast<StatusLightManager*>(ctx)->clearNightMode();
    };
    cbs->ctx = this;
}

bool StatusLightManager::resolveOverride(LedId led, bool autoLevel) const
{
    switch (static_cast<LedOverride>(getOverride(led))) {
        case LedOverride::ON:  return true;
        case LedOverride::OFF: return false;
        default:               return autoLevel;
    }
}

esp_err_t StatusLightManager::allLedsOff(void* ctx)
{
    auto self {static_cast<StatusLightManager*>(ctx)};
    for (LedOverride overwrite : self->overrides) {
        overwrite = LedOverride::OFF;
    }
    return ESP_OK;
}

esp_err_t StatusLightManager::allLedsAuto(void* ctx)
{
    auto self {static_cast<StatusLightManager*>(ctx)};
    for (LedOverride overwrite : self->overrides) {
        overwrite = LedOverride::AUTO;
    }
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
            modeLed = blink1Hz;
            break;

        case LedState::ZIGBEE_HOST_WAIT:
            pwrLed  = blink1Hz;
            modeLed = false;
            break;

        case LedState::NETWORK_DOWN:
            pwrLed  = blink3Hz;
            modeLed = false;
            break;

        case LedState::BOOTING:
            pwrLed  = true;
            modeLed = false;
            break;

        case LedState::ZIGBEE_ERROR:
            pwrLed  = blink1Hz;
            modeLed = blink3Hz;
            break;

        case LedState::FLASHING:
        case LedState::ERROR:
            pwrLed  = blink3Hz;
            modeLed = blink3Hz;
            break;
    }

    ledHal.setPwr(resolveOverride(LedId::PWR, pwrLed));
    ledHal.setMode(resolveOverride(LedId::MODE, modeLed));
}
