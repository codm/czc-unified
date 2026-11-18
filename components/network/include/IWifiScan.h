#ifndef IWIFISCAN_H_
#define IWIFISCAN_H_

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

/** @brief One access point found by a scan (deduplicated per SSID). */
struct ApInfo {
    char    ssid[33];   ///< Null-terminated SSID
    int8_t  rssi;       ///< Signal strength in dBm (strongest AP for this SSID)
    bool    secured;    ///< true if the AP requires authentication
};

/** @brief Progress of the current/last scan. */
enum class ScanState : uint8_t {
    Idle,       ///< No scan started yet (or STA was stopped)
    Running,    ///< Scan in progress
    Done,       ///< Results available via IWifiScan::results()
    Failed,     ///< Scan aborted by the driver
};

/**
 * @brief WiFi scan for the web portal.
 *
 * Independent of the state machine. Requires a started STA link
 * (true in SoftAp and in all WiFi states). Thread-safe.
 */
class IWifiScan
{
public:
    virtual ~IWifiScan() = default;

    /**
     * @brief Start an asynchronous scan. Poll scanState() for completion.
     * @return ESP_OK if started or already running,
     *         ESP_ERR_INVALID_STATE if STA is stopped,
     *         other esp_wifi errors (e.g. driver busy while connecting - retry later).
     */
    virtual esp_err_t startScan() = 0;

    /** @brief Current scan state. */
    virtual ScanState scanState() const = 0;

    /**
     * @brief Copy the last scan results.
     * @param out Destination array.
     * @param max Capacity of @p out.
     * @return Number of entries copied, sorted by RSSI (strongest first), max. 20.
     */
    virtual std::size_t results(ApInfo* out, std::size_t max) const = 0;
};

#endif // IWIFISCAN_H_
