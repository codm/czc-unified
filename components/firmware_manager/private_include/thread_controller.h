#ifndef CZC_THREAD_CONTROLLER_H_
#define CZC_THREAD_CONTROLLER_H_

#include "protocol_controller.h"

/**
 * @brief IProtocolController implementation for OpenThread border router mode.
 *
 *        Registers eventfd, configures the OT radio and host interfaces,
 *        starts the OT stack and the border router, then triggers auto-start
 *        if a dataset is already stored.
 *
 * @note  stop() performs a graceful Thread detach before halting the stack.
 *        The shared UART (UART_NUM_2) is owned by the OT Spinel driver for
 *        the entire lifetime of a running ThreadController.
 */
class ThreadController : public IProtocolController {
public:
    /**
     * @brief Constructor — initialises members to safe defaults.
     */
    ThreadController();

    /**
     * @brief Register eventfd VFS, configure and start the OpenThread stack
     *        and the border router.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t start() override;

    /**
     * @brief Gracefully detach from the Thread network, disable IPv6,
     *        and stop the OpenThread stack.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t stop() override;

    /**
     * @brief Query whether the OpenThread stack is currently running.
     *
     * @return true if running
     */
    bool isRunning() override;

    /**
     * @brief Hardware-resets the CC2652 via the RST pin.
     *
     *        Pure hardware pulse — does not touch OpenThread stack state.
     *        The Spinel driver already handles unsolicited RCP resets (see
     *        `rcpFailureHandler`), so the stack resynchronises on its own.
     *
     * @return `ESP_OK`
     */
    esp_err_t resetRcp() override;

    /**
     * @brief Erase the Thread dataset/settings and return to a clean state.
     *
     *        Unlike the Zigbee RCP, the CC2652 holds no meaningful config of
     *        its own in Thread mode — the dataset lives in the ESP's NVS via
     *        the OpenThread settings API. Disables the Thread/IPv6 interface,
     *        erases persistent info via `otInstanceErasePersistentInfo()`,
     *        then does a full `stop()` + `start()` to come back up clean.
     *
     * @return `ESP_OK` on success
     */
    esp_err_t factoryReset() override;

    /**
     * @brief The CC2652 has no RCP-hosted LED in Thread/OpenThread mode.
     *
     * @return `ESP_ERR_NOT_SUPPORTED`
     */
    esp_err_t setRcpLed(bool ledState) override;

private:
    bool threadActive;

    /** @brief Hardware-resets the CC2652 via the RST pin on RCP failure. */
    static void rcpFailureHandler();

    /** @brief Signals `aContext` (a SemaphoreHandle_t) once otThreadDetachGracefully() completes. */
    static void handleDetachGracefully(void* aContext);
};

#endif // CZC_THREAD_CONTROLLER_H_
