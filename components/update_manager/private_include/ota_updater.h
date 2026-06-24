#ifndef CZC_OTA_UPDATER_H_
#define CZC_OTA_UPDATER_H_

#include "esp_err.h"

/**
 * @brief Thin wrapper around esp_https_ota for ESP application firmware updates.
 *
 *        Downloads a valid ESP app image from the given URL, writes it to the
 *        inactive OTA partition, and reboots the device on success.
 *
 *        Can run live (no UART required) because it only needs network and the
 *        OTA partition — unlike RcpUpdater which needs exclusive UART access.
 *
 * @note  flash() spawns an internal FreeRTOS task so that it can be called from
 *        the HTTP server callback without blocking it.
 */
class OtaUpdater {
public:
    /**
     * @brief Constructor — initialises members to safe defaults.
     */
    OtaUpdater();

    /**
     * @brief Start an OTA update from the given URL.
     *
     *        Spawns a FreeRTOS task that downloads and validates the image.
     *        The task reboots the device on success or logs an error on failure.
     *
     * @param[in] url  HTTPS URL of the ESP firmware image (.bin)
     *
     * @return `ESP_OK` if the task was spawned successfully,
     *         `ESP_FAIL` if the task could not be created
     */
    esp_err_t flash(const char* url);

private:
    /**
     * @brief FreeRTOS task entry — runs the full esp_https_ota sequence.
     *
     * @param[in] pvParameters  Pointer to a heap-allocated url buffer (task deletes it)
     *
     * @return void
     */
    static void otaTask(void* pvParameters);
};

#endif // CZC_OTA_UPDATER_H_
