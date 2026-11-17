#include "WirelessAPI.h"

#include "esp_check.h"

const char* WirelessAPI::TAG = "wireless";

WirelessAPI::WirelessAPI()
    : softapConfig(), activeWirelessMode(ActiveWirelessMode::OFF), wifiIsConnected(false)
{
    ssid[0] = '\0';
    password[0] = '\0';
}

WirelessAPI::~WirelessAPI()
{
    if(activeWirelessMode != ActiveWirelessMode::OFF) {
        esp_wifi_stop();
        esp_wifi_deinit();
    }
}

void WirelessAPI::setWirelessConfig(const char* _ssid, const char* _password)
{
    strncpy(this->ssid, _ssid, sizeof(this->ssid) - 1);
    this->ssid[sizeof(this->ssid) - 1] = '\0';

    strncpy(this->password, _password, sizeof(this->password) - 1);
    this->password[sizeof(this->password) - 1] = '\0';
}

// -----------------------------------------------------------------------------
// AccessPoint
// -----------------------------------------------------------------------------

void WirelessAPI::initAccessPoint()
{
    if(!apNetif) apNetif = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(apNetif, "codm-otbr");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::ap_event_handler, this, NULL));

    wifi_config_t apConfig = {0};
    strncpy((char*)apConfig.ap.ssid,     softapConfig.ssid,     sizeof(apConfig.ap.ssid));
    strncpy((char*)apConfig.ap.password, softapConfig.password, sizeof(apConfig.ap.password));
    apConfig.ap.ssid_len       = strlen((char*)apConfig.ap.ssid);
    apConfig.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    apConfig.ap.max_connection = softapConfig.maxConnected;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Accesspoint started, SSID: %s — connect and open http://192.168.4.1 to configure", apConfig.ap.ssid);
    activeWirelessMode = ActiveWirelessMode::ACCESSPOINT;
}

void WirelessAPI::closeAccessPoint()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::ap_event_handler);
    esp_wifi_stop();
    esp_wifi_deinit();
    activeWirelessMode = ActiveWirelessMode::OFF;
}

// -----------------------------------------------------------------------------
// AP event handler + handlers
// -----------------------------------------------------------------------------

void WirelessAPI::ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    auto* self = static_cast<WirelessAPI*>(arg);
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED:
            self->onApStaConnected(static_cast<wifi_event_ap_staconnected_t*>(event_data));
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            self->onApStaDisconnected(static_cast<wifi_event_ap_stadisconnected_t*>(event_data));
            break;
    }
}

void WirelessAPI::onApStaConnected(wifi_event_ap_staconnected_t* event_data)
{
    ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event_data->mac), event_data->aid);
}

void WirelessAPI::onApStaDisconnected(wifi_event_ap_stadisconnected_t* event_data)
{
    ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d, reason=%d", MAC2STR(event_data->mac), event_data->aid, event_data->reason);
}

// -----------------------------------------------------------------------------
// Wifi (STA)
// -----------------------------------------------------------------------------

void WirelessAPI::initWifi()
{
    if(activeWirelessMode == ActiveWirelessMode::WIFI) {
        ESP_LOGW(TAG, "Wifi was already started");
        return;
    }

    if(!wifiNetif) wifiNetif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(wifiNetif, "codm-otbr");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::wifi_event_handler, this, NULL));

    wifi_config_t wifiConfig = {0};
    strncpy((char*)wifiConfig.sta.ssid,     this->ssid,     sizeof(wifiConfig.sta.ssid));
    strncpy((char*)wifiConfig.sta.password, this->password, sizeof(wifiConfig.sta.password));
    wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wifi STA initialised");
    activeWirelessMode = ActiveWirelessMode::WIFI;
}

void WirelessAPI::closeWifi()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::wifi_event_handler);
    esp_wifi_stop();
    esp_wifi_deinit();
    activeWirelessMode = ActiveWirelessMode::OFF;
}

void WirelessAPI::reconnect()
{
    esp_wifi_connect();
}

// -----------------------------------------------------------------------------
// Wifi (STA) event handler + handlers
// -----------------------------------------------------------------------------

void WirelessAPI::wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    auto* self = static_cast<WirelessAPI*>(arg);
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            self->onStaStart();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            self->onStaConnected();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            self->onStaDisconnected();
            break;
    }
}

void WirelessAPI::onStaStart()
{
    ESP_LOGI(TAG, "Wifi STA started, connecting...");
    esp_wifi_connect();
}

void WirelessAPI::onStaConnected()
{
    ESP_LOGI(TAG, "Wifi STA connected to AP");
}

void WirelessAPI::onStaDisconnected()
{
    ESP_LOGW(TAG, "Wifi STA disconnected");
    esp_event_post(NETWORK_EVENT, NETWORK_EVENT_WIFI_DISCONNECTED, nullptr, 0, portMAX_DELAY);
}

// -----------------------------------------------------------------------------
// Getters / Setters
// -----------------------------------------------------------------------------

const char* WirelessAPI::getSsid()
{
    return this->ssid;
}

const char* WirelessAPI::getPassword()
{
    return this->password;
}

void WirelessAPI::getCurrentIp(char* out, size_t out_size)
{
    out[0] = '\0';
    esp_netif_t* netif {(activeWirelessMode == ActiveWirelessMode::ACCESSPOINT ? apNetif : wifiNetif)};

    if (netif)
    {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK)
        {
            snprintf(out, out_size, IPSTR, IP2STR(&info.ip));
        }
    }
}

void WirelessAPI::setWifiIsConnected(bool connected)
{
    this->wifiIsConnected = connected;
}

bool WirelessAPI::getWifiIsConnected()
{
    return this->wifiIsConnected;
}

esp_err_t WirelessAPI::scan(scan_shortend_record_t **scan_records, uint16_t *count)
{
    esp_err_t ret = ESP_OK;
    bool temporary_sta_init   = !wifiNetif && !apNetif;
    bool temporary_sta_for_ap = !wifiNetif && apNetif;

    if (temporary_sta_init) {
        wifiNetif = esp_netif_create_default_wifi_sta();
        wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "Error initializing Wifi driver for scan");
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Error setting Wifi mode for scan");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Error starting Wifi driver for scan");
    } else if (temporary_sta_for_ap) {
        // Scanning requires station function, which plain WIFI_MODE_AP doesn't provide.
        // Add a STA interface and switch to APSTA just for the scan; the SoftAP keeps
        // running throughout and we drop back to AP-only again in cleanup.
        wifiNetif = esp_netif_create_default_wifi_sta();
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "Error switching to APSTA for scan");
    }

    constexpr wifi_country_t country_settings{
        .cc = "DE",
        .schan = 1,
        .nchan = 13,
        .max_tx_power = 20,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    wifi_scan_config_t scan_config {}; // default settings
    constexpr bool blocking_scan {true};
    uint16_t aps_found {0};
    wifi_ap_record_t* found_aps {nullptr};

    ESP_GOTO_ON_ERROR(esp_wifi_set_country(&country_settings), cleanup, TAG, "Error configuring Wifi country settings");
    ESP_GOTO_ON_ERROR(esp_wifi_scan_start(&scan_config, blocking_scan), cleanup, TAG, "Error during Wifi scan");

    ESP_GOTO_ON_ERROR(esp_wifi_scan_get_ap_num(&aps_found), cleanup, TAG, "Error retrieving found AP num");

    found_aps = (wifi_ap_record_t*)malloc(aps_found * sizeof(wifi_ap_record_t));
    ESP_GOTO_ON_FALSE(aps_found == 0 || found_aps != nullptr, ESP_ERR_NO_MEM, cleanup, TAG, "Out of memory for AP records");
    ESP_GOTO_ON_ERROR(esp_wifi_scan_get_ap_records(&aps_found, found_aps), cleanup, TAG, "Error getting AP records");

    *scan_records = (scan_shortend_record_t*)malloc(aps_found * sizeof(scan_shortend_record_t));
    ESP_GOTO_ON_FALSE(aps_found == 0 || *scan_records != nullptr, ESP_ERR_NO_MEM, cleanup, TAG, "Out of memory for scan results");

    for (size_t index = 0; index < aps_found; index++)
    {
        strncpy((char*)(*scan_records)[index].ssid, (const char*)found_aps[index].ssid, sizeof((*scan_records)[index].ssid) - 1);
        (*scan_records)[index].ssid[sizeof((*scan_records)[index].ssid) - 1] = '\0';
        (*scan_records)[index].rssi            = found_aps[index].rssi;
        (*scan_records)[index].primary_channel = found_aps[index].primary;
        (*scan_records)[index].authmode        = static_cast<uint8_t>(found_aps[index].authmode);
    }
    *count = aps_found;

cleanup:
    free(found_aps);
    if (temporary_sta_init)
    {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(wifiNetif);
        wifiNetif = nullptr;
    }
    else if (temporary_sta_for_ap)
    {
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_netif_destroy_default_wifi(wifiNetif);
        wifiNetif = nullptr;
    }
    return ret;
}

ActiveWirelessMode WirelessAPI::getActiveWirelessMode()
{
    return this->activeWirelessMode;
}
