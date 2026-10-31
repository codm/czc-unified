#ifndef TIME_SERVICE_NVS_BIND_H_
#define TIME_SERVICE_NVS_BIND_H_

#include <cstddef>
#include "esp_err.h"

/**
 * @brief NVS bindings for the TimeService defaults.
 *
 *        All keys live in the "time_service_defs" namespace.
 *        Every function opens and closes its own handle.
 */
namespace TimeServiceNvs
{
    /**
     * @brief Read the persisted default timezone.
     *
     * @param[out]    tz     Destination buffer for the timezone string
     * @param[in,out] length In: size of `tz` in bytes - Out: length of the string incl. terminator
     *
     * @return `ESP_OK` on success - `ESP_ERR_NVS_NOT_FOUND` if no timezone was stored yet
     *         (first boot) - `ESP_ERR_NVS_INVALID_LENGTH` if `tz` is too small
     */
    esp_err_t readDefaultTimezone(char* tz, size_t& length);

    /**
     * @brief Persist the default timezone applied on the next boot.
     *
     * @param[in] tz Timezone according to https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
     *
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on nullptr
     */
    esp_err_t writeDefaultTimezone(const char* tz);

    /**
     * @brief Read the persisted default time server.
     *
     * @param[out]    ts     Destination buffer for the server name
     * @param[in,out] length In: size of `ts` in bytes - Out: length of the string incl. terminator
     *
     * @return `ESP_OK` on success - `ESP_ERR_NVS_NOT_FOUND` if no server was stored yet
     *         (first boot) - `ESP_ERR_NVS_INVALID_LENGTH` if `ts` is too small
     */
    esp_err_t readDefaultTimeServer(char* ts, size_t& length);

    /**
     * @brief Persist the default time server used on the next boot.
     *
     * @param[in] ts Name of the server, e.g. "pool.ntp.org"
     *
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARG` on nullptr
     */
    esp_err_t writeDefaultTimeServer(const char* ts);

} // namespace TimeServiceNvs

#endif /* TIME_SERVICE_NVS_BIND_H_ */
