#ifndef ZSTACK_H_
#define ZSTACK_H_

#include "esp_err.h"
#include "driver/uart.h"

class ZstackMt
{
private:
    uart_port_t uartPort;
public:
    /** 
     * @brief Initialises ZstackMt object with UART_NUM_MAX as default
     */
    ZstackMt();
    ~ZstackMt();

    /**
     * @brief Takes given Uart and initialises with Zstack firmware baud
     * 
     * @param[in] _uartPort 
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else 
     */
    esp_err_t init(uart_port_t _uartPort);

    /**
     * @brief Deinit uart
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` otherwise 
     */
    esp_err_t close();

    /**
     * @brief Erases RCP Nvram 
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on no response - `ESP_FAIL` else
     */
    esp_err_t eraseNvram();
};

#endif // ZSTACK_H_
