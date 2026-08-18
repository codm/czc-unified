#include "zigbee_proxy_controller.h"

#include "board_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_check.h"

static const char* TAG = "ZigbeeProxyController";

ZigbeeProxyController::ZigbeeProxyController(DeviceMode proxyMode)
    : mode{proxyMode}, proxyActive{false}
{
    switch (proxyMode)
    {
    case DeviceMode::ZIGBEE_USB:
        transport = std::make_unique<UartTransport>();
        break;
    
    case DeviceMode::ZIGBEE_NET:
        transport = std::make_unique<TcpTransport>(Board::PROXY_TCP_PORT);
        break;
    
    case DeviceMode::ZIGBEE_ROUTER:
        //transport = std::make_unique<IProxyTransport>();
        break;

    default:
        //transport = std::make_unique<IProxyTransport>();
        break;
    }
}

void ZigbeeProxyController::rcpToHostFunc(void* ctx)
{
    auto* self = static_cast<ZigbeeProxyController*>(ctx);
    uint8_t buf[256];
    while (self->proxyActive) {
        int n = uart_read_bytes(Board::RCP_UART, buf, sizeof(buf), pdMS_TO_TICKS(10));
        if (n > 0) 
            self->transport->write(buf, n);
        vTaskDelay(pdTICKS_TO_MS(10));
    }
    vTaskDelete(nullptr);
}

void ZigbeeProxyController::hostToRcpFunc(void* ctx)
{
    auto* self = static_cast<ZigbeeProxyController*>(ctx);
    uint8_t buf[256];
    while (self->proxyActive) {
        int n = self->transport->read(buf, sizeof(buf));
        if (n > 0) 
            uart_write_bytes(Board::RCP_UART, buf, n);
        vTaskDelay(pdTICKS_TO_MS(10));
    }
    vTaskDelete(nullptr);
}

esp_err_t ZigbeeProxyController::start()
{
    if (!transport)
    {
        // ZIGBEE_ROUTER — RCP runs standalone, ESP doesn't touch the UART.
        return ESP_OK;
    }

    // RCP Uart init
    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };

    ESP_RETURN_ON_ERROR(uart_param_config(Board::RCP_UART, &cfg), TAG, "Failed at [RCP]uart_param_config");

    ESP_RETURN_ON_ERROR(uart_set_pin(Board::RCP_UART,
                        Board::RCP_UART_TX, Board::RCP_UART_RX,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG,
                        "Failed at [RCP]uart_set_pin"
                    );

    ESP_RETURN_ON_ERROR(uart_driver_install(Board::RCP_UART, 2048, 2048, 0, nullptr, 0),
                        TAG,
                        "Failed at [RCP]uart_driver_install"
                        );

    esp_err_t ret = transport->open();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Error starting Transport protocol");
        return ret;
    }

    proxyActive = true;
    xTaskCreate(rcpToHostFunc, "rcp_to_host", 4096, this, 5, &rcpToHostTask);
    xTaskCreate(hostToRcpFunc, "host_to_rcp", 4096, this, 5, &hostToRcpTask);

    return ESP_OK;
}

esp_err_t ZigbeeProxyController::stop()
{
    if (!transport)
    {
        // ZIGBEE_ROUTER — nothing was started.
        return ESP_OK;
    }

    proxyActive = false;

    if (rcpToHostTask) 
    { 
        vTaskDelete(rcpToHostTask); 
        rcpToHostTask = nullptr; 
    }
    if (hostToRcpTask) 
    { 
        vTaskDelete(hostToRcpTask); 
        hostToRcpTask = nullptr; 
    }

    transport->close();

    return uart_driver_delete(Board::HOST_UART);
}

bool ZigbeeProxyController::isRunning()
{
    return proxyActive;
}

esp_err_t ZigbeeProxyController::resetRcp()
{
    constexpr uint32_t rebootTimeoutMs {5000};

    ESP_RETURN_ON_ERROR(stop(), TAG, "stop before RCP reset failed");

    esp_err_t err = zstackMt.init(Board::RCP_UART);
    if (err == ESP_OK) {
        err = zstackMt.rebootRcp(rebootTimeoutMs);
    }
    zstackMt.close();

    esp_err_t startErr = start();
    return (err == ESP_OK) ? startErr : err;
}

esp_err_t ZigbeeProxyController::factoryReset()
{
    ESP_RETURN_ON_ERROR(stop(), TAG, "stop before RCP erase failed");

    esp_err_t err = zstackMt.init(Board::RCP_UART);
    if (err == ESP_OK) {
        err = zstackMt.eraseNvram();
    }
    zstackMt.close();

    esp_err_t startErr = start();
    return (err == ESP_OK) ? startErr : err;
}
