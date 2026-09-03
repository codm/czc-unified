#include "time_service.h"

#include "time_service.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <cstring>

namespace {
    constexpr TickType_t SYNC_TIMEOUT   = pdMS_TO_TICKS(10000);
    constexpr int        MIN_VALID_YEAR = 2024 - 1900;

    const char* TAG = "TimeService";

    char s_server[64] = "pool.ntp.org";

    esp_err_t waitForSync()
    {
        if (esp_netif_sntp_sync_wait(SYNC_TIMEOUT) != ESP_OK) {
            ESP_LOGW(TAG, "no sync from '%s'", s_server);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "time synced with '%s'", s_server);
        return ESP_OK;
    }
}

namespace TimeService {

esp_err_t init(const char* server)
{
    if (server != nullptr) 
        strlcpy(s_server, server, sizeof(s_server));

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(s_server);
    cfg.server_from_dhcp  = false;

    esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        return waitForSync();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sntp_init: %s", esp_err_to_name(err));
        return err;
    }
    if (setTimezone("CET-1CEST,M3.5.0,M10.5.0/3") != ESP_OK) {
        ESP_LOGE(TAG, "sntp_init: error setting default timezone");
        return err;
    } // Source of Default timezone should be NVS! 

    return waitForSync();
}

esp_err_t setServer(const char* server)
{
    if (server == nullptr || strlen(server) >= sizeof(s_server)) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(s_server, server, sizeof(s_server));

    esp_sntp_setservername(0, s_server);
    esp_sntp_restart();
    return waitForSync();
}

esp_err_t setTimezone(const char* tz)
{
    if (tz == nullptr) return ESP_ERR_INVALID_ARG;
    setenv("TZ", tz, 1);
    tzset();
    return ESP_OK;
}

esp_err_t getCurrTime(struct tm& currTime)
{
    time_t now {0};
    time(&now);
    localtime_r(&now, &currTime);

    return (currTime.tm_year < MIN_VALID_YEAR) ? ESP_FAIL : ESP_OK;
}

} /* TimeService */
