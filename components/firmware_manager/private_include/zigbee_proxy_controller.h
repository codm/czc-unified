#ifndef CZC_ZIGBEE_PROXY_CONTROLLER_H_
#define CZC_ZIGBEE_PROXY_CONTROLLER_H_

#include "protocol_controller.h"
#include "firmware_manager.h"
#include "proxy_transport.h"
#include "zstack_mt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <memory>

/**
 * @brief IProtocolController implementation for Zigbee coordinator proxy mode.
 *
 *        Acts as a transparent serial proxy between the CC2652 RCP UART and a
 *        host application via IProxyTransport (USB or TCP).
 *
 *        The active transport is selected at construction based on DeviceMode:
 *          - ZIGBEE_USB    → UartTransport
 *          - ZIGBEE_NET    → TcpTransport
 *          - ZIGBEE_ROUTER → none — RCP runs standalone, start()/stop() are no-ops
 *
 */
class ZigbeeProxyController : public IProtocolController {
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

    /**
     * @brief Hardware-resets the RCP via `ZstackMt`, using the Z-Stack MT
     *        protocol the RCP's Zigbee firmware speaks.
     *
     *        Stops the proxy (releasing the UART) for the duration of the
     *        reset and restarts it afterwards.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t resetRcp() override;

    /**
     * @brief Erases the RCP's Z-Stack NVRAM (network table & config) via
     *        `ZstackMt`.
     *
     *        Stops the proxy (releasing the UART) for the duration of the
     *        erase and restarts it afterwards.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t factoryReset() override;

    /** 
     * @brief Reacts on status_light_manager events and sets ZIGBEE LED accordingly
     * 
     * @param[in] ledState future state for the LED
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else
     * 
     * @note Pauses Proxy tasks
     */ 
    esp_err_t setRcpLed(bool ledState) override;

private:
    static void rcpToHostFunc(void* ctx);
    static void hostToRcpFunc(void* ctx);

    const char* RCP_TO_HOST_HANDLE = "rcp_to_host";
    const char* HOST_TO_RCP_HANDLE = "host_to_rcp";

    DeviceMode                       mode;
    bool                             proxyActive;
    std::unique_ptr<IProxyTransport> transport;
    TaskHandle_t                     rcpToHostTask{nullptr};
    TaskHandle_t                     hostToRcpTask{nullptr};
    ZstackMt                         zstackMt;
    SemaphoreHandle_t                rcpAccessMutex;
};

#endif // CZC_ZIGBEE_PROXY_CONTROLLER_H_
