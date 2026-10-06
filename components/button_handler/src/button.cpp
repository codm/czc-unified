#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "button.h"
#include "status_light_manager.h"
#include "board_config.h"









namespace {

    static const char *TAG = "BUTTON";

    static uint32_t press_start_time = 0;

    static StatusLightManager* statusLightManager = nullptr;

    void status_led_toggle()
    {
        if (!statusLightManager) {
            ESP_LOGW(TAG, "StatusLightManager not initialized");
            return;
        }

        ESP_LOGI(TAG, "0-3 Sec Press: Toggle Status-LED");
        bool enabled = !statusLightManager->isLedOutputEnabled();

        statusLightManager->setLedOutputEnabled(enabled);

        ESP_LOGI(TAG, "Status-LED %s", enabled ? "ON" : "OFF");
    }


    void usb_network_toggle()
    {
        ESP_LOGI(TAG, "3-5 Sec Press: Toggle USB/Network Mode");
    }


    void trigger_factory_reset()
    {
        ESP_LOGI(TAG, "10+ Sec Press: Trigger Factory Reset");
    }



    void handle_long_press(uint32_t press_time_ms)
    {
        ESP_LOGI(TAG, "Button held for: %d ms", press_time_ms);
        if (press_time_ms < 3000) {
            status_led_toggle();
        }
        else if (press_time_ms < 5000) {
            usb_network_toggle();
        }
        else if (press_time_ms >= 10000) {
            trigger_factory_reset();
        }
    }

    void button_press_down_cb(void *arg, void *usr_data)
    {
        press_start_time = esp_log_timestamp();
    }


    void button_press_up_cb(void *arg, void *usr_data)
    {
        uint32_t press_time = esp_log_timestamp() - press_start_time;

        handle_long_press(press_time);
    }
}


namespace Button {
    void init(StatusLightManager& statusLight)
    {
        statusLightManager = &statusLight;

        button_config_t button_config = {
            .long_press_time = 1000,
            .short_press_time = 50,
        };


        button_gpio_config_t gpio_config = {
            .gpio_num = Board::BTN_PIN,
            .active_level = 0,
            .enable_power_save = false,
            .disable_pull = true,
        };


        button_handle_t btn = nullptr;

        esp_err_t ret = iot_button_new_gpio_device(
            &button_config,
            &gpio_config,
            &btn
        );

        if (ret != ESP_OK || btn == nullptr) {
            ESP_LOGE(TAG, "Button could not be created: %s",
                    esp_err_to_name(ret));
            return;
        }



        ret = iot_button_register_cb(
            btn,
            BUTTON_PRESS_DOWN,
            nullptr,
            button_press_down_cb,
            nullptr
        );

        ret = iot_button_register_cb(
            btn,
            BUTTON_PRESS_UP,
            nullptr,
            button_press_up_cb,
            nullptr
        );

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Callback failed: %s",
                    esp_err_to_name(ret));
            return;
        }

        ESP_LOGI(TAG, "Button initialized on GPIO%d", Board::BTN_PIN);
    }
}