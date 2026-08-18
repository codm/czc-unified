#include "zstack_mt.h"

#include "nvram_addr.h"
#include "board_config.h"

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/task.h"

constexpr const char* TAG = "ZstackMt";

constexpr uint8_t SOF {0xFE}; 
constexpr uint16_t PACKET_WAIT_TIME_MS {1000};

esp_err_t ZstackMt::sendCmdAndWaitForResponse(Request& request, uint8_t length = 0)
{
    esp_err_t ret {sendPacket(request)};
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error on sendPacket");
        return ESP_FAIL;
    }
    
    uint8_t requestMeta[3] {static_cast<uint8_t>(request.cmd0 + 0x40), request.cmd1, length};

    ret = listenForPacket(PACKET_WAIT_TIME_MS, request);
    if (ret != ESP_OK)  {
        ESP_LOGE(TAG, "Error on packet receive - %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    if (request.cmd0 != requestMeta[0] || request.cmd1 != requestMeta[1] || (requestMeta[2] > 0 && request.len != requestMeta[2])) {
        ESP_LOGW(TAG, "Received Answer to different Request - Or error in Request receive");
        ESP_LOGW(TAG, "Expected: Cmd0: 0x%hx | Cmd1: 0x%hx | Length: 0x%hx", requestMeta[0], requestMeta[1], requestMeta[2]);
        ESP_LOGW(TAG, "Received: Cmd0: 0x%hx | Cmd1: 0x%hx | Length: 0x%hx", request.cmd0 & 0xFF, request.cmd1 & 0xFF, request.len & 0xFF);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;   
}

esp_err_t ZstackMt::sendPacket(const Request &request)
{
    if (uart_write_bytes(uartPort, &SOF, sizeof(SOF)) < 0) 
        return ESP_FAIL;

    if (uart_write_bytes(uartPort, &request.len, sizeof(request.len)) < 0) 
        return ESP_FAIL;
 
    if (uart_write_bytes(uartPort, &request.cmd0, sizeof(request.cmd0)) < 0) 
        return ESP_FAIL;

    if (uart_write_bytes(uartPort, &request.cmd1, sizeof(request.cmd1)) < 0) 
        return ESP_FAIL;
    
    if (request.len > 0 && uart_write_bytes(uartPort, request.data, request.len) < 0)
        return ESP_FAIL;

    uint8_t checksum {calcChecksum(request)};
    ESP_LOGD(TAG, "Send complete - Calculated Checksum: %d", checksum);
    if (uart_write_bytes(uartPort, &checksum, sizeof(checksum)) < 0) 
        return ESP_FAIL;
    
    return ESP_OK;
}

esp_err_t ZstackMt::listenForPacket(uint32_t timeoutMs, Request& receivedRequest)
{
    const TickType_t startTicks{xTaskGetTickCount()};
    const TickType_t timeoutTicks{pdMS_TO_TICKS(timeoutMs)};

    while ((xTaskGetTickCount() - startTicks) < timeoutTicks) {
        size_t available{0};
        uart_get_buffered_data_len(uartPort, &available);

        if (available >= 1) {
            uint8_t readSingle{0};
            if (uart_read_bytes(uartPort, &readSingle, 1, timeoutTicks) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }

            if (readSingle != SOF) {
                ESP_LOGD(TAG, "IVE READ SOMETHING ....");
                continue;
            }

            if (uart_read_bytes(uartPort, &readSingle, 1, timeoutTicks) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }
            receivedRequest.len = readSingle;

            if (uart_read_bytes(uartPort, &receivedRequest.cmd0, sizeof(receivedRequest.cmd0), timeoutTicks) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }

            if (uart_read_bytes(uartPort, &receivedRequest.cmd1, sizeof(receivedRequest.cmd1), timeoutTicks) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }

            if (uart_read_bytes(uartPort, receivedRequest.data, receivedRequest.len, timeoutTicks) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }

            if (uart_read_bytes(uartPort, &readSingle, 1, pdMS_TO_TICKS(10)) < 0) {
                ESP_LOGW(TAG, "Uart Error!");
                return ESP_FAIL;
            }
            if (readSingle != calcChecksum(receivedRequest)) {
                ESP_LOGW(TAG, "Invalid Checksum on package receive");
                return ESP_ERR_INVALID_CRC;
            }

            return ESP_OK;
        }
        vTaskDelay(1);
    }

    ESP_LOGW(TAG, "Timeout waiting for response (%lu ms)", timeoutMs);
    return ESP_ERR_TIMEOUT;
}

esp_err_t ZstackMt::waitForResetCallback(uint32_t timeoutMs = 5000)
{
    Request request;
    ESP_RETURN_ON_ERROR(listenForPacket(timeoutMs, request), TAG, "Error waitForResetCallback listenForPackage");

    if ((request.cmd0 != 0x41) || (request.cmd1 != 0x80) || request.len != 0x06) {
        ESP_LOGW(TAG, "waitForResetCallback - received wrong command!");
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGD(TAG, "Received SYS_RESET_IND MT_SYS Callback!");

    return ESP_OK;
}

uint8_t ZstackMt::calcChecksum(const Request &request)
{
    uint8_t acc {static_cast<uint8_t>(request.cmd0 ^ request.cmd1 ^ request.len)};
    for (size_t index = 0; index < request.len; ++index) {
        acc ^= request.data[index];
    }

    return acc;
}

esp_err_t ZstackMt::ping()
{
    Request request {
        .cmd0 = 0x21,
        .cmd1 = 0x01,
        .data = 0,
        .len = 0x00,
    };
    ESP_LOGD(TAG, "Sending PING");
    ESP_RETURN_ON_ERROR(sendCmdAndWaitForResponse(request), TAG, "PING command did not work properly");
    ESP_LOGD(TAG, "PING answered correctly!");

    return ESP_OK;
}

esp_err_t ZstackMt::setNvramClearStartops()
{
    Request request {
        .cmd0 = 0x21,
        .cmd1 = 0x09,
        .data {0x03, 0x00, 0x00, 0x01, 0x03},  // Id=0x0003 LE, Offset, Len, Value
        .len = 0x05,
    };
    ESP_LOGD(TAG, "Sending NVRAMCLEARSTARTOPS");
    ESP_RETURN_ON_ERROR(sendCmdAndWaitForResponse(request), TAG, "PING command did not work properly");

    if (request.data[0] != ESP_OK) {
        ESP_LOGW(TAG, "NVRAMCLEARSTARTOPS return value = failure");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGD(TAG, "NVRAMCLEARSTARTOPS completed successfully!");
    return ESP_OK;
}

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

    gpio_config_t rstConf{};
    rstConf.pin_bit_mask = (1ULL << Board::CC_RST_PIN);
    rstConf.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&rstConf), TAG, "RST GPIO config failed");

    gpio_config_t bslConf{};
    bslConf.pin_bit_mask = (1ULL << Board::CC_BSL_PIN);
    bslConf.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&bslConf), TAG, "BSL GPIO config failed");
    gpio_set_drive_capability(Board::CC_BSL_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_level(Board::CC_BSL_PIN, 1);

    // Reboot into non bsl
    gpio_set_level(Board::CC_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(Board::CC_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

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
                        
    ESP_LOGD(TAG, "Init OK — UART%d at %lu baud", uartPort, Board::BSL_BAUD);
    uart_flush_input(uartPort);
    waitForResetCallback();

    return ESP_OK;
}

esp_err_t ZstackMt::close()
{
    return uart_driver_delete(uartPort);
}

esp_err_t ZstackMt::eraseNvram()
{
    ESP_RETURN_ON_ERROR(setNvramClearStartops(), TAG, "Error setting Nvram Clear Start options");
    ESP_RETURN_ON_ERROR(rebootRcp(5 * PACKET_WAIT_TIME_MS), TAG, "RCP not back up in time");

    return ESP_OK;
}

esp_err_t ZstackMt::rebootRcp(uint32_t timeoutMs)
{
    gpio_set_level(Board::CC_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(Board::CC_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_RETURN_ON_ERROR(waitForResetCallback(timeoutMs), TAG, "Reset Callback was not received in time");
    return ESP_OK;
}
