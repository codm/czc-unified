#include "status_light_hal.h"

#include "status_light_event.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "StatusLightHal";

esp_err_t StatusLightHal::init()
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << Board::LED_PWR_PIN) | (1ULL << Board::LED_MODE_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "LED GPIO config failed");

    setPwr(false);
    setMode(false);
    return ESP_OK;
}

void StatusLightHal::setPwr(bool enabled)
{
    gpio_set_level(Board::LED_PWR_PIN, enabled ? 1 : 0);
}

void StatusLightHal::setMode(bool enabled)
{
    gpio_set_level(Board::LED_MODE_PIN, enabled ? 1 : 0);
}

void StatusLightHal::setRcp(bool enabled)
{
    esp_event_post(RCP_LED_EVENT, enabled ? 1 : 0, nullptr, 0, 0);
}
