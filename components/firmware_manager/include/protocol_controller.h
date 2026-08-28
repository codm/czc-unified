#ifndef CZC_PROTOCOL_CONTROLLER_H_
#define CZC_PROTOCOL_CONTROLLER_H_

#include "esp_err.h"

/**
 * @brief Abstract interface for a protocol stack controller.
 *
 *        FirmwareManager holds a pointer to the active implementation
 *        (ThreadController or ZigbeeProxyController) and drives it
 *        through this interface.
 *
 * @note  virtual dispatch is intentional here — the mode switch is a cold
 *        path (reboot-gated) so vtable cost is irrelevant.
 */
class IProtocolController {
public:
    /**
     * @brief Start the protocol stack.
     *
     * @return `ESP_OK` on success
     */
    virtual esp_err_t start() = 0;

    /**
     * @brief Stop the protocol stack gracefully.
     *
     * @return `ESP_OK` on success
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief Query whether the stack is currently running.
     *
     * @return true if running, false otherwise
     */
    virtual bool isRunning() = 0;

    /**
     * @brief Hardware-reset the RCP without touching the protocol stack's
     *        own state (no re-attach, no dataset/network-table changes).
     *
     * @return `ESP_OK` on success
     */
    virtual esp_err_t resetRcp() = 0;

    /**
     * @brief Erase the RCP's persisted network config and return it to a
     *        clean, unconfigured state.
     *
     * @return `ESP_OK` on success
     */
    virtual esp_err_t factoryReset() = 0;

    /**
     * @brief Set the RCP-hosted status LED, if the active protocol has one.
     *
     * @param[in] ledState  true = LED on, false = LED off
     *
     * @return `ESP_OK` on success — `ESP_ERR_NOT_SUPPORTED` if the protocol
     *         has no RCP-hosted LED — `ESP_ERR_INVALID_STATE` if the RCP
     *         link isn't currently available
     */
    virtual esp_err_t setRcpLed(bool ledState) = 0;

    virtual ~IProtocolController() = default;
};

#endif // CZC_PROTOCOL_CONTROLLER_H_
