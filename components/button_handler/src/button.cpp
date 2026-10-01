#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BUTTON";

static void button_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "Taster gedrückt!");
}








void meine_steuerung_init(void)
{
    // Allgemeine Button-Konfiguration
    button_config_t button_config = {
        .long_press_time = 10000,
        .short_press_time = 50,
    };

    // GPIO-Konfiguration
    button_gpio_config_t gpio_config = {
        .gpio_num = GPIO_NUM_35,
        .active_level = 1,
        .enable_power_save = false,
        .disable_pull = true,
    };

    // GPIO-Button erstellen
    button_handle_t btn = nullptr;

    esp_err_t ret = iot_button_new_gpio_device(
        &button_config,
        &gpio_config,
        &btn
    );

    if (ret != ESP_OK || btn == nullptr) {
        ESP_LOGE(TAG, "Button konnte nicht erstellt werden: %s",
                 esp_err_to_name(ret));
        return;
    }

    // Single-Click Callback
    ret = iot_button_register_cb(
        btn,
        BUTTON_SINGLE_CLICK,
        nullptr,
        button_click_cb,
        nullptr
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Callback konnte nicht registriert werden: %s",
                 esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Button auf GPIO35 initialisiert");
}