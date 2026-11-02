#ifndef ZSTACK_H_
#define ZSTACK_H_

#include "esp_err.h"
#include "driver/uart.h"

constexpr uint8_t REQ_MAX_DATA_LEN {250};

struct Request {
    uint8_t cmd0 = 0;
    uint8_t cmd1 = 0;
    uint8_t data[REQ_MAX_DATA_LEN] {};
    uint8_t len  = 0;
};

class ZstackMt
{
private:
    uart_port_t uartPort;

    /**
     * @brief Sends given request and wait for response. Received response transferred into given request.
     * 
     * @param[inout] request [in] command to be send - [out] received package
     * @param[in] length expected length value of the SRSP - defaults to 0 -> wont get checked
     * 
     * @return `ESP_OK` on success - `ESP_INVALID_RESPONSE` on wrong packet receive - `ESP_FAIL` on send / receive error
     * 
     * @note Uses timeouts defined in zstack_mt.cpp file
     * 
     * @warning Should only be used for SREQ Commands
     */
    esp_err_t sendCmdAndWaitForResponse(Request& request, uint8_t length);

    /**
     * @brief   Send a Zstack Monitor and Test packet
     * 
     *          Start of Frame (0xFE) + Command + Frame Check Sequence (XOR of all bytes in message)
     *
     *          Command: 
     *  
     *          |   1 Byte    |   2 Bytes   |   0 - 250 Bytes |
     * 
     *          | data_length |   Command   |       Data      |
     * 
     * @param[in] request reference to build request object
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARGS` faulty dataAndCmd - `ESP_FAIL` otherwise  
     */
    esp_err_t sendPacket(const Request& request);

    /**
     * @brief   Listens for data on the uart blocking until timeout expires - saves the data of received packet in buffer 
     * 
     * @param[in] timeoutMs maximum waiting time for data
     * @param[out] receivedRequest request received on uart 
     * 
     * @returns `ESP_OK` on successful read - `ESP_FAIL` on Uart error - 
     *          `ESP_ERR_INVALID_ARG` on falsely configured buffer - `ESP_ERR_TIMEOUT` no package received in timeout window - 
     *          `ESP_ERR_INVALID_CRC` on checksum error or incomplete message
     */
    esp_err_t listenForPacket(uint32_t timeoutMs, Request& receivedRequest);

    /**
     * @brief Waits (blocking) for SYS_RESET_IND Callback -> Successful RCP start
     * 
     * @param[in] timeoutMs maximum waiting time - 5s default
     * 
     * @returns `ESP_OK` on success - `ESP_ERR_INVALID_RESPONSE` on different packet received - 
     *           listenForPacket error code otherwise  
     */
    esp_err_t waitForResetCallback(uint32_t timeoutMs);

    /**
     * @brief Calculates Zstack Checksum - Xor from length to last data byte 
     * 
     * @param[in] request Request to calculate the checksum from
     * 
     * @return Calculated checksum 
     * 
     * @warning Doesnt check request - always retuns a value
     */
    uint8_t calcChecksum(const Request& request);

    /** 
     * @brief Sends ping cmd 
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else
    */
    esp_err_t ping();

    /**
     * @brief Sets ZCD_STARTOPT_CLEAR_CONFIG & ZCD_STARTOPT_CLEAR_STATE
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` on send or receive error - `ESP_ERR_INVALID_RESPONSE` on failure response
     */
    esp_err_t setNvramClearStartops(); 

public:
    /**
     * @brief Initialises ZstackMt object with the given UART port.
     *
     * @param[in] port  UART port this instance operates on (default: unset)
     */
    explicit ZstackMt(uart_port_t port = UART_NUM_MAX);
    ~ZstackMt();

    /**
     * @brief   Takes given Uart and initialises with Zstack firmware baud and configures rst & bsl gpios (else pull down)
     *          Reboots RCP into normal operation
     * 
     * @param[in] _uartPort 
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else 
     * 
     * @note Reboots RCP and waits for the reset callback - blocking - could take a second or two
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
     * @return `ESP_OK` on success - `ESP_FAIL` else
     */
    esp_err_t eraseNvram();

    /** @brief Reboots RCP and waits for the Reset callback
     *
     * @param[in] timeoutMs max poll time in ms
     *
     * @return `ESP_OK` when the RCP is back up in time - `ESP_FAIL` when the RCP doesnt answer in time
     *
     * @warning UART has to be configured for ZstackMt for a valid answer
    */
    esp_err_t rebootRcp(uint32_t timeoutMs);

    /**
     * @brief Sets the RCP-hosted status LED (LED index 1) via a vendor Z-Stack MT command.
     *
     *        Uses the UART port this instance was constructed with (or last
     *        passed to `init()`) — does not touch RST/BSL GPIOs and does not
     *        install/remove the UART driver, so it is safe to call while the
     *        port is already installed and in use elsewhere, as long as
     *        nothing else is reading from it concurrently.
     *
     * @param[in] ledState  true = LED on, false = LED off
     *
     * @return `ESP_OK` on success - `ESP_FAIL` on send/receive error - `ESP_ERR_INVALID_RESPONSE` on failure status
     */
    esp_err_t setLed(bool ledState);
};

#endif // ZSTACK_H_
