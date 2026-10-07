#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "button.h"
#include "status_light_manager.h"
#include "board_config.h"


static const char* TAG = "Button";

Button::Button(StatusLightManager& statusLight)
    : statusLight_(statusLight) {}

Button::~Button() {
    if (btn_handle_) {
        iot_button_delete(btn_handle_);
    }
}


esp_err_t Button::init() {
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
        &btn_handle_
    );

    if (ret != ESP_OK || btn_handle_ == nullptr) {
        ESP_LOGE(TAG, "Button could not be created: %s",
                esp_err_to_name(ret));
        return ret;
    }



    ret = iot_button_register_cb(
        btn_handle_,
        BUTTON_PRESS_DOWN,
        nullptr,
        press_down_cb_adapter,
        this
    );

    ret = iot_button_register_cb(
        btn_handle_,
        BUTTON_PRESS_UP,
        nullptr,
        press_up_cb_adapter,
        this
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Callback failed: %s",
                esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Button initialized on GPIO%d", Board::BTN_PIN);
    return ESP_OK;
}

void Button::press_down_cb_adapter(void *arg, void *usr_data) {
    auto* self = static_cast<Button*>(usr_data);
    if (self) {
        self->on_press_down();
    }
}

void Button::press_up_cb_adapter(void *arg, void *usr_data) {
    auto* self = static_cast<Button*>(usr_data);
    if (self) {
        self->on_press_up();
    }
}

void Button::on_press_down(void) {
    press_start_time_ = esp_log_timestamp();
}

void Button::on_press_up(void) {
    uint32_t press_time = esp_log_timestamp() - press_start_time_;
    handle_long_press(press_time);
}

void Button::handle_long_press(uint32_t press_time_ms)
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

void Button::status_led_toggle()
{
    ESP_LOGI(TAG, "0-3 Sec Press: Toggle Status-LED");
    bool enabled = !statusLight_.isLedOutputEnabled();

    statusLight_.setLedOutputEnabled(enabled);

    ESP_LOGI(TAG, "Status-LED %s", enabled ? "ON" : "OFF");
}


void Button::usb_network_toggle()
{
    ESP_LOGI(TAG, "3-5 Sec Press: Toggle USB/Network Mode");
}


void Button::trigger_factory_reset()
{
    ESP_LOGI(TAG, "10+ Sec Press: Trigger Factory Reset");
}



