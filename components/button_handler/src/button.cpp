#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BUTTON";

static uint32_t press_start_time = 0;






// Irgendwie muss hier eine Zeitmessung rein also der Gedankengang ist:
// button wird gedrückt->Zeitmessung startet->button wird losgelassen->handler Funktion ruft die passende Funktion auf


/** Das soll passieren wenn der Button gedrückt wird
 * 
 * 
 * 0 ───── 3s ───── 5s ───────── 10s ───────>
 * │        │         │             │
 * │  Led   │Umschalt │ nichts      │ Werkseinstellungen
 * │ an/aus │USB/Netz │             │   
 * 
*/




static void status_led_toggle()
{
    ESP_LOGI(TAG, "0-3 Sekunden gedrückt: Status-LED umschalten");
}


static void usb_network_toggle()
{
    ESP_LOGI(TAG, "3-5 Sekunden gedrückt: Zwischen USB und Netzbetrieb umschalten");
}


static void trigger_factory_reset()
{
    ESP_LOGI(TAG, "10+ Sekunden gedrückt: Werkseinstellungen zurücksetzen");
}



static void handle_long_press(uint32_t press_time_ms)
{
    ESP_LOGI(TAG, "Taster lange gedrückt: %d ms", press_time_ms);
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

static void button_press_down_cb(void *arg, void *usr_data)
{
    press_start_time = esp_log_timestamp();
}


static void button_press_up_cb(void *arg, void *usr_data)
{
    uint32_t press_time = esp_log_timestamp() - press_start_time;

    handle_long_press(press_time);
}


void meine_steuerung_init(void)
{

    button_config_t button_config = {
        .long_press_time = 1000,
        .short_press_time = 50,
    };


    button_gpio_config_t gpio_config = {
        .gpio_num = GPIO_NUM_35,
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
        ESP_LOGE(TAG, "Button konnte nicht erstellt werden: %s",
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
        ESP_LOGE(TAG, "Callback konnte nicht registriert werden: %s",
                 esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Button auf GPIO35 initialisiert");
}