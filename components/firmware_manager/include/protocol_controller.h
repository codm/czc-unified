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

    virtual ~IProtocolController() = default;
};

#endif // CZC_PROTOCOL_CONTROLLER_H_
