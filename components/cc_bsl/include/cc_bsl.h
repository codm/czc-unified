#ifndef CC_BSL_H
#define CC_BSL_H

#include "esp_err.h"
#include "driver/uart.h"

/**
 * @brief BSL transport HAL for the CC2652P7 co-processor.
 *
 *        Owns the UART driver and the RST/BSL GPIO lines for the duration
 *        of a flash session. Exactly one owner: RcpUpdater.
 *
 *        The UART port number is supplied via init() — not a compile-time
 *        constant — so the owner decides which port to use.
 *
 * @note  The BSL protocol uses: SIZE / CHECKSUM / CMD [/ DATA],
 *        ACK = 0xCC, NACK = 0x33 (both preceded by 0x00).
 */
class CcBsl {
public:
    /**
     * @brief Constructor — members are initialised to safe defaults.
     */
    CcBsl();

    /**
     * @brief Configure RST/BSL GPIO outputs and install the UART driver.
     *
     * @param[in] uartNum  UART port to use for BSL communication
     *
     * @return `ESP_OK` on success, propagated driver error otherwise
     */
    esp_err_t init(uart_port_t uartNum);

    /**
     * @brief Toggle RST/BSL pins to put the CC2652 into ROM bootloader mode,
     *        then synchronise the UART with the BSL.
     *
     * @note  Idempotent — skips the toggle sequence if already in BSL mode.
     *
     * @return `ESP_OK` on successful sync, `ESP_FAIL` if no ACK received
     */
    esp_err_t enterBootloader();

    /**
     * @brief Send BANK_ERASE (0x2C) — erases the entire CC2652 flash.
     *
     * @note  Calls enterBootloader() if not already in BSL mode.
     *        Timeout is 10 s to allow for the full erase cycle.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t eraseFlash();

    /**
     * @brief Send DOWNLOAD (0x21) — tells the BSL the flash target address and size.
     *
     * @param[in] address  Start address in CC2652 flash (usually 0x00000000)
     * @param[in] size     Total firmware size in bytes — must be a multiple of 4
     *
     * @return `ESP_OK` on success, `ESP_ERR_INVALID_ARG` if size is not 4-byte aligned
     */
    esp_err_t beginFlash(uint32_t address, uint32_t size);

    /**
     * @brief Send SEND_DATA (0x24) — transmit one data block to the BSL.
     *
     * @param[in] data    Pointer to data buffer
     * @param[in] length  Number of bytes to send — must be ≤ 252
     *
     * @return `ESP_OK` on success
     */
    esp_err_t sendData(const uint8_t* data, int length);

    /**
     * @brief Hardware-reset the CC2652 via the RST pin and exit BSL mode.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t reset();

    /**
     * @brief Release the UART driver so other consumers (e.g. OpenThread) can use the port.
     *
     * @return `ESP_OK` on success, forwarded error from `uart_driver_delete` otherwise
     */
    esp_err_t close();

private:
    uart_port_t uartPort;
    bool        bslMode;

    /**
     * @brief Delete any existing UART driver and reinstall it fresh at BSL baud.
     *
     * @warning Must be called immediately before BSL communication, not during
     *          init(). The CC2652 transmits while not in BSL filling up the 
     *          Rx Buffer making a waitForAck() unreliable.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t acquireUart();

    /**
     * @brief Frame and transmit a BSL packet: [SIZE][CHECKSUM][CMD][DATA…]
     *
     * @param[in] cmdAndData  Byte 0 = CMD, bytes 1…n = payload
     * @param[in] length      Total length of cmdAndData
     *
     * @return `ESP_OK` on success
     */
    esp_err_t sendPacket(const uint8_t* cmdAndData, size_t length);

    /**
     * @brief Send the UART sync byte (0x55 0x55) and wait for ACK.
     *
     * @return `ESP_OK` on ACK, `ESP_FAIL` on timeout or NACK
     */
    esp_err_t uartSync();

    /**
     * @brief Wait for a 2-byte ACK (0x00 0xCC) or NACK (0x00 0x33).
     *
     * @param[in] timeoutMs  Maximum wait time in milliseconds
     *
     * @return true on ACK, false on NACK or timeout
     */
    bool waitAck(uint32_t timeoutMs);

    /**
     * @brief Read a BSL response packet, verify checksum, and send ACK/NACK.
     *
     * @param[out] outBuf   Buffer to receive the response payload
     * @param[in]  bufSize  Size of outBuf
     * @param[out] outLen   Actual number of bytes written to outBuf
     *
     * @return `ESP_OK` on success, `ESP_ERR_INVALID_CRC` on checksum mismatch
     */
    esp_err_t readResponse(uint8_t* outBuf, size_t bufSize, size_t* outLen);

    /**
     * @brief Send GET_STATUS (0x23) and verify the last command succeeded.
     *
     * @return `ESP_OK` if BSL reports 0x40 (success), `ESP_FAIL` otherwise
     */
    esp_err_t checkLastCmd();
};

#endif // CC_BSL_H
