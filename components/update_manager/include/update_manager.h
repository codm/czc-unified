#ifndef CZC_UPDATE_MANAGER_H_
#define CZC_UPDATE_MANAGER_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief Manages RCP and ESP firmware updates with OTA-partition mutual exclusion.
 *
 *        Owns a RcpUpdater (TI binary, boot-time only) and an OtaUpdater
 *        (ESP app image, live). Both use the same OTA partition — a `busy` flag
 *        prevents them from running simultaneously.
 *
 *        Intended usage:
 *          - flashRcp() is called at boot time by AppController when
 *            `rcp_flash_pending` is set in NVS.
 *          - flashEsp() is called live from the web API callback.
 */
class UpdateManager {
public:
    /**
     * @brief Constructor — initialises members to safe defaults.
     */
    UpdateManager();

    /**
     * @brief Download and flash RCP firmware from `url` via the BSL protocol.
     *
     *        Non-blocking — spawns an internal task that runs the full
     *        download→erase→program→reset sequence. Call waitForRcpFlash() to
     *        block until it finishes and get the result.
     *
     *        Must only be started before any other UART user is active — e.g.
     *        at boot time, or during first-time setup before
     *        FirmwareManager::start() has ever been called.
     *
     * @param[in] url  HTTPS URL of the TI firmware binary (.bin)
     *
     * @return `ESP_OK` if the task was spawned successfully,
     *         `ESP_ERR_INVALID_STATE` if another update is already in progress,
     *         `ESP_FAIL` if the task could not be created
     */
    esp_err_t flashRcp(const char* url);

    /**
     * @brief Block until the RCP flash task started by flashRcp() finishes.
     *
     * @param[in] timeout  Maximum time to wait
     *
     * @return The result of the flash (`ESP_OK` on success),
     *         `ESP_ERR_TIMEOUT` if `timeout` elapsed first
     */
    esp_err_t waitForRcpFlash(TickType_t timeout = portMAX_DELAY);

    /**
     * @brief Start an OTA update for the ESP firmware from `url`.
     *
     *        Non-blocking — spawns an internal task. The task reboots the device
     *        on success.
     *
     * @param[in] url  HTTPS URL of the ESP firmware image (.bin)
     *
     * @return `ESP_OK` if the update task was started,
     *         `ESP_ERR_INVALID_STATE` if another update is already in progress
     */
    esp_err_t flashEsp(const char* url);

private:
    bool busy;
    SemaphoreHandle_t rcpFlashDone;
    esp_err_t rcpFlashResult;

    /**
     * @brief FreeRTOS task entry — runs the blocking RCP flash and signals rcpFlashDone.
     *
     * @param[in] pvParameters  Pointer to a heap-allocated task params struct (task deletes it)
     *
     * @return void
     */
    static void rcpFlashTask(void* pvParameters);
};

#endif // CZC_UPDATE_MANAGER_H_
