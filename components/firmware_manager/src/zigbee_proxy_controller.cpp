#include "zigbee_proxy_controller.h"

#include "board_config.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char* TAG = "ZigbeeProxyController";

ZigbeeProxyController::ZigbeeProxyController(DeviceMode proxyMode)
    : mode{proxyMode}, proxyActive{false}
{
    if (mode == DeviceMode::ZIGBEE_USB)
        transport = std::make_unique<UartTransport>();
    else
        transport = std::make_unique<TcpTransport>(Board::PROXY_TCP_PORT);
}

void ZigbeeProxyController::rcpToHostFunc(void* ctx)
{
    auto* self = static_cast<ZigbeeProxyController*>(ctx);
    uint8_t buf[256];
    while (self->proxyActive) {
        int n = uart_read_bytes(Board::RCP_UART, buf, sizeof(buf), pdMS_TO_TICKS(10));
        if (n > 0) self->transport->write(buf, n);
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
        if (n > 0) uart_write_bytes(Board::RCP_UART, buf, n);
        vTaskDelay(pdTICKS_TO_MS(10));
    }
    vTaskDelete(nullptr);
}

esp_err_t ZigbeeProxyController::start()
{
    savedVprintf = esp_log_set_vprintf([](const char*, va_list) -> int { return 0; });

    esp_err_t ret = transport->open();
    if (ret != ESP_OK) {
        esp_log_set_vprintf(savedVprintf);
        savedVprintf = nullptr;
        return ret;
    }

    proxyActive = true;
    xTaskCreate(rcpToHostFunc, "rcp_to_host", 4096, this, 5, &rcpToHostTask);
    xTaskCreate(hostToRcpFunc, "host_to_rcp", 4096, this, 5, &hostToRcpTask);

    return ESP_OK;
}

esp_err_t ZigbeeProxyController::stop()
{
    proxyActive = false;

    if (rcpToHostTask) { vTaskDelete(rcpToHostTask); rcpToHostTask = nullptr; }
    if (hostToRcpTask) { vTaskDelete(hostToRcpTask); hostToRcpTask = nullptr; }

    transport->close();

    if (savedVprintf) {
        esp_log_set_vprintf(savedVprintf);
        savedVprintf = nullptr;
    }
    return ESP_OK;
}

bool ZigbeeProxyController::isRunning()
{
    return proxyActive;
}
