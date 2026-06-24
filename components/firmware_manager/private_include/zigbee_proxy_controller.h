#ifndef CZC_ZIGBEE_PROXY_CONTROLLER_H_
#define CZC_ZIGBEE_PROXY_CONTROLLER_H_

#include "protocol_controller.h"
#include "proxy_transport.h"
#include "firmware_manager.h"

/**
 * @brief ProtocolController implementation for Zigbee coordinator proxy mode.
 *
 *        Acts as a transparent serial proxy between the CC2652 RCP UART and a
 *        host application via IProxyTransport (USB or TCP).
 *
 *        The active transport is selected at construction based on DeviceMode:
 *          - ZIGBEE_USB → UartTransport
 *          - ZIGBEE_NET → TcpTransport
 *
 * @note  Not yet implemented — stub only.
 */
class ZigbeeProxyController : public ProtocolController {
public:
    /**
     * @brief Constructor.
     *
     * @param[in] mode  Must be ZIGBEE_USB or ZIGBEE_NET
     */
    explicit ZigbeeProxyController(DeviceMode mode);

    /**
     * @brief Start the proxy — initialise transport and begin pumping bytes.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t start() override;

    /**
     * @brief Stop the proxy and release the UART.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t stop() override;

    /**
     * @brief Query whether the proxy is currently running.
     *
     * @return true if running
     */
    bool isRunning() override;

private:
    DeviceMode  mode;
    bool        proxyActive;
};

#endif // CZC_ZIGBEE_PROXY_CONTROLLER_H_
