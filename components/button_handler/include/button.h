#pragma once

#include "driver/gpio.h"
#include "iot_button.h"

class StatusLightManager;

class Button {
public:
    Button(StatusLightManager& statusLightManager);
    ~Button();

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    esp_err_t init();

private:
    void on_press_down();
    void on_press_up();
    void handle_long_press(uint32_t press_time_ms);

    void status_led_toggle();
    void usb_network_toggle();
    void trigger_factory_reset();
    
    static void press_down_cb_adapter(void *arg, void *usr_data);
    static void press_up_cb_adapter(void *arg, void *usr_data);

    gpio_num_t pin_;
    StatusLightManager& statusLight_;
    button_handle_t btn_handle_{nullptr};
    uint32_t press_start_time_{0};
};