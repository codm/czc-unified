#ifndef CZC_RCP_UPDATER_H_
#define CZC_RCP_UPDATER_H_

#include "esp_err.h"
#include "rcp_hal.h"

/**
 * @brief Orchestrates a full RCP firmware update: download → stage → BSL-flash.
 *
 *        Downloads the TI binary from the given URL into the inactive OTA
 *        partition (used as staging), then drives the CC2652 BSL via RcpHal
 *        to erase, program and reset the co-processor.
 *
 *        Runs exclusively at boot time from the `rcp_flash_pending` intent
 *        so the shared UART is conflict-free.
 */
class RcpUpdater {
public:
    /**
     * @brief Constructor — initialises members to safe defaults.
     */
    RcpUpdater();

    /**
     * @brief Download firmware from `url` and flash it to the CC2652.
     *
     *        Sequence: downloadToStaging() → RcpHal::eraseFlash() →
     *        RcpHal::beginFlash() → RcpHal::sendData() (loop) → RcpHal::reset()
     *
     * @param[in] url  HTTPS URL of the TI firmware binary (.bin)
     *
     * @return `ESP_OK` on success
     */
    esp_err_t flash(const char* url);

private:
    RcpHal hal;
    size_t downloadedSize;

    /**
     * @brief Download the firmware binary from `url` into the inactive OTA partition.
     *
     *        The OTA partition is used as raw staging storage — the image is a TI
     *        binary, not an ESP app image, so esp_https_ota cannot be used here.
     *
     * @param[in] url  HTTPS URL of the TI firmware binary
     *
     * @return `ESP_OK` on success, HTTP or partition error otherwise
     */
    esp_err_t downloadToStaging(const char* url);

    /**
     * @brief Read firmware from the staging partition and transmit it block-by-block
     *        to the CC2652 via RcpHal::sendData().
     *
     * @return `ESP_OK` on success
     */
    esp_err_t flashFromStaging();
};

#endif // CZC_RCP_UPDATER_H_
