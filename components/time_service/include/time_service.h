#ifndef NTP_H_
#define NTP_H_

#include <ctime>
#include "esp_err.h"

namespace TimeService
{
    /**
     * @brief initializes SNTP default server "pool.ntp.org" with NETIF interface (Threat save)
     * 
     * @param[in] server Name of the server - if not specified using default server "pool.ntp.org" 
     * 
     * @return `ESP_OK` on successful connection within 10s - `ESP_FAIL` else
     */
    esp_err_t init(const char* server = nullptr);

    /**
     * @brief updates timeserver and checks if its reachable 
     * 
     * @param[in] server Name of the server 
     * 
     * @return `ESP_OK` on successful connection within 10s - `ESP_FAIL` else
     */
    esp_err_t setServer(const char* server);

    /**
     * @brief change timezone environment variables
     * 
     * @param[in] tz Timezones according to https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
     * 
     * @return `ESP_OK` on success - `ESP_ERR_INVALID_ARGS` on faulty timezone string
     */
    esp_err_t setTimezone(const char* tz);

    /**
     * @brief gets current time in form of 
     * 
     * @param[out] currTime current time as a struct
     * 
     * @return `ESP_OK` on success - `ESP_FAIL` otherwise
     */
    esp_err_t getCurrTime(struct tm& currTime);

} // namespace TimeService

#endif /* NTP_H_ */
