#include "cc_bsl.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "cc_bsl";

// BSL command bytes (CC2652 ROM bootloader protocol)
constexpr uint8_t CMD_PING         = 0x20;
constexpr uint8_t CMD_DOWNLOAD     = 0x21;
constexpr uint8_t CMD_GET_STATUS   = 0x23;
constexpr uint8_t CMD_SEND_DATA    = 0x24;
constexpr uint8_t CMD_BANK_ERASE   = 0x2C;
constexpr uint8_t CMD_UART_SYNC    = 0x55;

constexpr uint8_t BSL_ACK  = 0xCC;
constexpr uint8_t BSL_NACK = 0x33;

constexpr size_t MAX_SEND_DATA_LEN = 252;  // 255 - SIZE - CHECKSUM

CcBsl::CcBsl()
    : uartPort{UART_NUM_MAX}, bslMode{false}
{}

esp_err_t CcBsl::init(uart_port_t uartNum)
{
    uartPort = uartNum;

    if (uart_is_driver_installed(uartPort)) {
        ESP_LOGW(TAG, "UART%d driver already installed at init: deleting stale instance!", uartPort);
        uart_driver_delete(uartPort);
    }

    gpio_config_t rstConf{};
    rstConf.pin_bit_mask = (1ULL << Board::CC_RST_PIN);
    rstConf.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&rstConf), TAG, "RST GPIO config failed");
    gpio_set_level(Board::CC_RST_PIN, 1);

    gpio_config_t bslConf{};
    bslConf.pin_bit_mask = (1ULL << Board::CC_BSL_PIN);
    bslConf.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&bslConf), TAG, "BSL GPIO config failed");
    gpio_set_drive_capability(Board::CC_BSL_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_level(Board::CC_BSL_PIN, 1);

    uart_config_t uartCfg{};
    uartCfg.baud_rate  = Board::BSL_BAUD;
    uartCfg.data_bits  = UART_DATA_8_BITS;
    uartCfg.parity     = UART_PARITY_DISABLE;
    uartCfg.stop_bits  = UART_STOP_BITS_1;
    uartCfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uartCfg.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_param_config(uartPort, &uartCfg), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(uartPort, Board::RCP_UART_TX, Board::RCP_UART_RX,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(uartPort, 1024, 0, 0, nullptr, 0),
                        TAG, "uart_driver_install failed");
    uart_flush_input(uartPort);

    ESP_LOGD(TAG, "Init OK — UART%d at %lu baud", uartPort, Board::BSL_BAUD);
    return ESP_OK;
}

esp_err_t CcBsl::acquireUart()
{
    if (uart_is_driver_installed(uartPort)) {
        uart_driver_delete(uartPort);
        ESP_LOGD(TAG, "UART driver deleted for fresh BSL acquire");
    }

    uart_config_t uartCfg{};
    uartCfg.baud_rate  = Board::BSL_BAUD;
    uartCfg.data_bits  = UART_DATA_8_BITS;
    uartCfg.parity     = UART_PARITY_DISABLE;
    uartCfg.stop_bits  = UART_STOP_BITS_1;
    uartCfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uartCfg.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_param_config(uartPort, &uartCfg), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(uartPort, Board::RCP_UART_TX, Board::RCP_UART_RX,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(uartPort, 1024, 0, 0, nullptr, 0),
                        TAG, "uart_driver_install failed");
    uart_flush_input(uartPort);

    ESP_LOGI(TAG, "UART%d acquired at %lu baud", uartPort, Board::BSL_BAUD);
    return ESP_OK;
}

esp_err_t CcBsl::enterBootloader()
{
    ESP_RETURN_ON_ERROR(acquireUart(), TAG, "UART acquire failed");

    if (!bslMode) {
        ESP_LOGD(TAG, "RST=0, BSL=0");
        gpio_set_level(Board::CC_RST_PIN, 0);
        gpio_set_level(Board::CC_BSL_PIN, 0);
        ESP_LOGI(TAG, "BSL pin readback: RST=%d BSL=%d",
                 gpio_get_level(Board::CC_RST_PIN), gpio_get_level(Board::CC_BSL_PIN));
        vTaskDelay(pdMS_TO_TICKS(50));

        ESP_LOGD(TAG, "RST=1 — CC2652 samples BSL pin now");
        gpio_set_level(Board::CC_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGD(TAG, "BSL=1");
        gpio_set_level(Board::CC_BSL_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_RETURN_ON_ERROR(uartSync(), TAG, "UART sync failed");

    bslMode = true;
    return ESP_OK;
}

esp_err_t CcBsl::eraseFlash()
{
    if (!bslMode) {
        ESP_RETURN_ON_ERROR(enterBootloader(), TAG, "enterBootloader failed");
    }

    ESP_LOGI(TAG, "BANK_ERASE...");
    uint8_t cmd{CMD_BANK_ERASE};
    ESP_RETURN_ON_ERROR(sendPacket(&cmd, 1), TAG, "BANK_ERASE send failed");

    if (!waitAck(10000)) {
        ESP_LOGE(TAG, "BANK_ERASE: no ACK");
        return ESP_FAIL;
    }
    return checkLastCmd();
}

esp_err_t CcBsl::beginFlash(uint32_t address, uint32_t size)
{
    if ((size % 4) != 0) {
        ESP_LOGE(TAG, "beginFlash: size %lu is not 4-byte aligned", size);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t payload[9]{
        CMD_DOWNLOAD,
        static_cast<uint8_t>((address >> 24) & 0xFF),
        static_cast<uint8_t>((address >> 16) & 0xFF),
        static_cast<uint8_t>((address >>  8) & 0xFF),
        static_cast<uint8_t>((address >>  0) & 0xFF),
        static_cast<uint8_t>((size    >> 24) & 0xFF),
        static_cast<uint8_t>((size    >> 16) & 0xFF),
        static_cast<uint8_t>((size    >>  8) & 0xFF),
        static_cast<uint8_t>((size    >>  0) & 0xFF),
    };

    ESP_RETURN_ON_ERROR(sendPacket(payload, sizeof(payload)), TAG, "DOWNLOAD send failed");
    if (!waitAck(2000)) {
        ESP_LOGE(TAG, "DOWNLOAD: no ACK");
        return ESP_FAIL;
    }
    return checkLastCmd();
}

esp_err_t CcBsl::sendData(const uint8_t* data, int length)
{
    if (data == nullptr || length <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (length > static_cast<int>(MAX_SEND_DATA_LEN)) {
        ESP_LOGE(TAG, "sendData: block too large (%d > %d)", length, MAX_SEND_DATA_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t packet[1 + MAX_SEND_DATA_LEN]{};
    packet[0] = CMD_SEND_DATA;
    memcpy(&packet[1], data, length);

    ESP_RETURN_ON_ERROR(sendPacket(packet, 1 + length), TAG, "SEND_DATA send failed");
    if (!waitAck(10000)) {
        ESP_LOGE(TAG, "SEND_DATA: no ACK");
        return ESP_FAIL;
    }
    return checkLastCmd();
}

esp_err_t CcBsl::reset()
{
    gpio_set_level(Board::CC_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(Board::CC_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    bslMode = false;
    return ESP_OK;
}

esp_err_t CcBsl::close()
{
    return uart_driver_delete(uartPort);
}

// --- private ---

esp_err_t CcBsl::sendPacket(const uint8_t* cmdAndData, size_t length)
{
    if (length == 0 || cmdAndData == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t size{static_cast<uint8_t>(length + 2)};  // +2 for SIZE and CHECKSUM bytes
    uint8_t checksum{0};
    for (size_t i{0}; i < length; i++) {
        checksum += cmdAndData[i];
    }

    uint8_t header[2]{size, checksum};
    if (uart_write_bytes(uartPort, header, sizeof(header)) < 0) {
        return ESP_FAIL;
    }
    if (uart_write_bytes(uartPort, cmdAndData, length) < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t CcBsl::uartSync()
{
    uint8_t syncBytes[2]{CMD_UART_SYNC, CMD_UART_SYNC};
    if (uart_write_bytes(uartPort, syncBytes, sizeof(syncBytes)) < 0) {
        return ESP_FAIL;
    }
    if (!waitAck(8000)) {
        ESP_LOGE(TAG, "UART sync: no ACK");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool CcBsl::waitAck(uint32_t timeoutMs)
{
    TickType_t start{xTaskGetTickCount()};

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeoutMs)) {
        size_t available{0};
        uart_get_buffered_data_len(uartPort, &available);

        if (available >= 2) {
            uint8_t buf[2]{};
            if (uart_read_bytes(uartPort, buf, 2, pdMS_TO_TICKS(10)) == 2) {
                if (buf[0] == 0x00 && buf[1] == BSL_ACK)  return true;
                if (buf[0] == 0x00 && buf[1] == BSL_NACK) {
                    ESP_LOGW(TAG, "BSL NACK received");
                    return false;
                }
            }
        }
        vTaskDelay(1);
    }

    ESP_LOGW(TAG, "Timeout waiting for ACK (%lu ms)", timeoutMs);
    return false;
}

esp_err_t CcBsl::readResponse(uint8_t* outBuf, size_t bufSize, size_t* outLen)
{
    if (outBuf == nullptr || outLen == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *outLen = 0;

    uint8_t header[2]{};
    if (uart_read_bytes(uartPort, header, 2, pdMS_TO_TICKS(500)) != 2) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t packetSize{header[0]};
    uint8_t checksum{header[1]};

    if (packetSize < 2) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    size_t dataLen{static_cast<size_t>(packetSize - 2)};
    if (dataLen > bufSize) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (dataLen > 0) {
        if (uart_read_bytes(uartPort, outBuf, dataLen, pdMS_TO_TICKS(500))
                != static_cast<int>(dataLen)) {
            uint8_t nack[2]{0x00, BSL_NACK};
            uart_write_bytes(uartPort, nack, 2);
            return ESP_ERR_TIMEOUT;
        }
    }

    uint8_t calcSum{0};
    for (size_t i{0}; i < dataLen; i++) {
        calcSum += outBuf[i];
    }

    if (calcSum != checksum) {
        uint8_t nack[2]{0x00, BSL_NACK};
        uart_write_bytes(uartPort, nack, 2);
        return ESP_ERR_INVALID_CRC;
    }

    uint8_t ack[2]{0x00, BSL_ACK};
    uart_write_bytes(uartPort, ack, 2);

    *outLen = dataLen;
    return ESP_OK;
}

esp_err_t CcBsl::checkLastCmd()
{
    uint8_t cmd{CMD_GET_STATUS};
    ESP_RETURN_ON_ERROR(sendPacket(&cmd, 1), TAG, "GET_STATUS send failed");

    if (!waitAck(500)) {
        return ESP_FAIL;
    }

    uint8_t resp[4]{};
    size_t  respLen{0};
    ESP_RETURN_ON_ERROR(readResponse(resp, sizeof(resp), &respLen), TAG, "GET_STATUS read failed");

    if (respLen < 1) {
        return ESP_FAIL;
    }
    if (resp[0] == 0x40) {
        return ESP_OK;
    }

    const char* errStr{"unknown"};
    switch (resp[0]) {
        case 0x41: errStr = "Unknown command"; break;
        case 0x42: errStr = "Invalid command"; break;
        case 0x43: errStr = "Invalid address"; break;
        case 0x44: errStr = "Flash fail";      break;
    }
    ESP_LOGE(TAG, "BSL error 0x%02X: %s", resp[0], errStr);
    return ESP_FAIL;
}
