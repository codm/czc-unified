#include "zstack_mt.h"

#include "nvram_addr.h"
#include "board_config.h"

#include "esp_log.h"
#include "esp_check.h"

constexpr char* TAG = "ZstackMt";

ZstackMt::ZstackMt(/* args */)
    : uartPort(UART_NUM_MAX)
{
}

ZstackMt::~ZstackMt()
{
    uart_driver_delete(uartPort);
}

esp_err_t ZstackMt::init(uart_port_t _uartPort)
{
    uartPort = _uartPort;

    if (uart_is_driver_installed(uartPort)) {
        ESP_LOGW(TAG, "UART%d driver already installed at init: deleting stale instance!", uartPort);
        uart_driver_delete(uartPort);
    }

    const uart_config_t uart_config {
        .baud_rate  = Board::COORD_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_RETURN_ON_ERROR(uart_param_config(uartPort, &uart_config), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(uartPort, Board::RCP_UART_TX, Board::RCP_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(uartPort, 1024, 0, 0, nullptr, 0),
                        TAG, "uart_driver_install failed");

    ESP_LOGD(TAG, "Init OK — UART%d at %lu baud", uartPort, Board::COORD_BAUD);
    return ESP_OK;
}

esp_err_t ZstackMt::close()
{
    return uart_driver_delete(uartPort);
}

esp_err_t ZstackMt::eraseNvram()
{
    
    return esp_err_t();
}
