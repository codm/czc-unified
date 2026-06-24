#include "zigbee_proxy_controller.h"

#include "esp_log.h"

static const char* TAG = "ZigbeeProxyController";

// --- UartTransport stub ---

int UartTransport::write(const uint8_t* /*buf*/, size_t /*len*/) { return -1; }
int UartTransport::read(uint8_t* /*buf*/, size_t /*len*/)        { return -1; }

// --- TcpTransport stub ---

int TcpTransport::write(const uint8_t* /*buf*/, size_t /*len*/) { return -1; }
int TcpTransport::read(uint8_t* /*buf*/, size_t /*len*/)        { return -1; }

// --- ZigbeeProxyController ---

ZigbeeProxyController::ZigbeeProxyController(DeviceMode proxyMode)
    : mode{proxyMode}, proxyActive{false}
{}

esp_err_t ZigbeeProxyController::start()
{
    ESP_LOGW(TAG, "ZigbeeProxyController not yet implemented");
    proxyActive = true;
    return ESP_OK;
}

esp_err_t ZigbeeProxyController::stop()
{
    proxyActive = false;
    return ESP_OK;
}

bool ZigbeeProxyController::isRunning()
{
    return proxyActive;
}
