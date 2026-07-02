#include "WirelessAPI.h"

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

ActiveWirelessMode WirelessAPI::getActiveWirelessMode()
{
    return this->activeWirelessMode;
}
