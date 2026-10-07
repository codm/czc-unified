#ifndef BUTTON_H_
#define BUTTON_H_

#include <functional>
#include "driver/gpio.h"
#include "iot_button.h"

class StatusLightManager;
class AppController;

/** 
 * @brief A class to handle button press events via GPIO on Board::BTN_PIN.
 * There are different actions based on the duration of the button press:
 * 
 * 0 ───── 3s ───── 5s ───────>
 * │        │         │
 * │  Led   │Toggle   │ Nothing
 * │ on/off │USB/Netw.│
 * 
 * 
 * @note If the button is pressed at startup for over 10 seconds, a factory reset is triggered.
 */

class Button {
public:
    /**
     * @brief Constructor for the Button class
     * 
     * @param[in] statusLightManager Reference to the StatusLightManager instance
     * @param[in] appController Reference to the AppController instance
     */
    Button(StatusLightManager& statusLightManager, AppController& appController);
    ~Button();

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    /** @brief Initialize the button
     * 
     * @return esp_err_t ESP_OK if successful, otherwise an error code
     */
    esp_err_t init();

private:
    /** @brief Handle the button press down event
     * 
     */
    void on_press_down();
    /** @brief Handle the button press up event
     * 
     */
    void on_press_up();
    /** @brief Handle the long press event
     * 
     * @param press_time_ms The duration of the press in milliseconds
     */
    void handle_long_press(uint32_t press_time_ms);

    /** @brief Toggle the status LED
     * 
     */
    void status_led_toggle();
    /** @brief Toggle the USB network connection
     * 
     */
    void usb_network_toggle();
    /** @brief Trigger the factory reset
     * 
     */
    void trigger_factory_reset();

    /** @brief Adapter function for the button press down callback
     * 
     * @param arg Argument passed to the callback
     * @param usr_data User data passed to the callback
     */
    static void press_down_cb_adapter(void *arg, void *usr_data);
    /** @brief Adapter function for the button press up callback
     * 
     * @param arg Argument passed to the callback
     * @param usr_data User data passed to the callback
     */
    static void press_up_cb_adapter(void *arg, void *usr_data);

    gpio_num_t pin_;
    StatusLightManager& statusLight_;
    AppController& appController_;
    button_handle_t btn_handle_{nullptr};
    uint32_t press_start_time_{0};
};


#endif