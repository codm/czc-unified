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

private:
    bool threadActive;

    /** @brief Hardware-resets the CC2652 via the RST pin on RCP failure. */
    static void rcpFailureHandler();

    /** @brief Signals `aContext` (a SemaphoreHandle_t) once otThreadDetachGracefully() completes. */
    static void handleDetachGracefully(void* aContext);
};

#endif // CZC_THREAD_CONTROLLER_H_
