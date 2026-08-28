#ifndef CZC_FIRMWARE_MANAGER_H_
#define CZC_FIRMWARE_MANAGER_H_

#include "protocol_controller.h"
#include "status_light_event.h"
#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <memory>

/**
 * @brief Operating modes of the device.
 *
 *        Stored in NVS and read at boot by AppController to determine
 *        which IProtocolController to start.
 */
enum class DeviceMode : int32_t {
    THREAD       = 0,  ///< OpenThread border router (OTBR)
    ZIGBEE_USB   = 1,  ///< Zigbee coordinator — USB serial proxy to host
    ZIGBEE_NET   = 2,  ///< Zigbee coordinator — TCP proxy to network host
    ZIGBEE_ROUTER = 3, ///< Zigbee router — RCP runs standalone, ESP does nothing
};

/**
 * @brief Selects and drives the active protocol stack (Thread or Zigbee proxy).
 *
 *        Owns a ThreadController and a ZigbeeProxyController as static
 *        instances. start() activates exactly one of them based on the
 *        requested DeviceMode, enforcing the "exactly one UART owner" invariant.
 *
 * @note  ZIGBEE_ROUTER requires no controller — the RCP operates independently.
 */
class FirmwareManager {
public:
    /**
     * @brief Constructor — initialises members to safe defaults. 
     * 
     * @warning No protocol defined yet!
     */
    FirmwareManager();

    /**
     * @brief Subscribe to `RCP_LED_EVENT` and start the RCP-LED worker task.
     *
     *        Mirrors the already-arbitrated RCP LED on/off state onto the
     *        active protocol's RCP-hosted LED (if it has one) via
     *        `IProtocolController::setRcpLed()`. The worker task does the
     *        actual (blocking) call, so the event loop's own task is never
     *        blocked.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t init();

    /**
     * @brief Start the protocol stack for the given mode.
     *
     *        Selects the matching IProtocolController and calls its start().
     *        Must not be called while any controller is already running.
     *        For ZIGBEE_ROUTER mode, no controller is started.
     *
     * @param[in] mode  The protocol mode to activate
     *
     * @return `ESP_OK` on success
     */
    esp_err_t start(DeviceMode mode);

    /**
     * @brief Stops the protocol Stack currently running.
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` else
     */
    esp_err_t stop();

    /**
     * @brief Return the currently active device mode.
     *
     * @return `DeviceMode` that was passed to the last successful start()
     */
    DeviceMode getActiveMode();

    /**
     * @brief Hardware-reset the RCP without touching the active protocol's
     *        own state. Forwards to the active `IProtocolController`.
     *
     * @return `ESP_OK` on success — `ESP_ERR_INVALID_STATE` if no protocol is running
     */
    esp_err_t resetRcp();

    /**
     * @brief Erase the RCP's persisted network config via the active
     *        `IProtocolController`.
     *
     * @return `ESP_OK` on success — `ESP_ERR_INVALID_STATE` if no protocol is running
     */
    esp_err_t factoryReset();

private:
    std::unique_ptr<IProtocolController> protocol;
    DeviceMode activeMode;

    QueueHandle_t rcpLedQueue{nullptr};
    TaskHandle_t  rcpLedTask{nullptr};

    static void ledEventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static void rcpLedTaskFunc(void* arg);
};

#endif // CZC_FIRMWARE_MANAGER_H_
