#include "WirelessAPI.h"

const char* WirelessAPI::TAG = "wireless";

WirelessAPI::WirelessAPI(char *_accessPointSsid, char *_accessPointPassword, uint8_t _accessPointMaxConnected)
    : accessPointSsid(_accessPointSsid), accessPointPassword(_accessPointPassword),accessPointMaxConnected(_accessPointMaxConnected) ,activeWirelessMode(ActiveWirelessMode::OFF), wifiIsConnected(false)
{
}

WirelessAPI::~WirelessAPI()
{
    esp_wifi_deinit();
}

void WirelessAPI::setWirelessConfig(char *_ssid, char *_password)
{
    this->ssid = _ssid;
    this->password = _password;
}

void WirelessAPI::initAccessPoint()
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t initAccessPointConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initAccessPointConfig));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::ap_event_handler, NULL, NULL));

    // config
    wifi_config_t accessPointConfig = {0};
    strncpy((char*)accessPointConfig.ap.ssid, this->ssid, sizeof(accessPointConfig.ap.ssid));
    strncpy((char*)accessPointConfig.ap.password, this->password, sizeof(accessPointConfig.ap.password));
    accessPointConfig.ap.ssid_len = strlen((char*)accessPointConfig.ap.ssid);
    accessPointConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
    accessPointConfig.ap.max_connection = accessPointMaxConnected;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &accessPointConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Accesspoint started and open, SSID: %s Psw: %s", accessPointConfig.ap.ssid, accessPointConfig.ap.password);
    activeWirelessMode = ActiveWirelessMode::ACCESSPOINT;
}

void WirelessAPI::closeAccessPoint()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::ap_event_handler);
    esp_wifi_stop();
    activeWirelessMode = ActiveWirelessMode::OFF;
}

void WirelessAPI::initWifi()
{
    if(activeWirelessMode == ActiveWirelessMode::WIFI) {
        ESP_LOGW(TAG, "Wifi was allready started");
        return;
    }

    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t initWifiConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initWifiConfig));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::wifi_event_handler, NULL, NULL));

    wifi_config_t wifiConfig = {0};
    strncpy((char*)wifiConfig.sta.ssid, this->ssid, sizeof(wifiConfig.ap.ssid));
    strncpy((char*)wifiConfig.sta.password, this->password, sizeof(wifiConfig.ap.password));
    wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    activeWirelessMode = ActiveWirelessMode::WIFI;
}

void WirelessAPI::closeWifi()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessAPI::wifi_event_handler);
    esp_wifi_stop();
    activeWirelessMode = ActiveWirelessMode::OFF;
}

void WirelessAPI::setWifiIsConnected(bool _wifiIsConnected)
{
    this->wifiIsConnected = _wifiIsConnected;
}

bool WirelessAPI::getWifiIsConnected()
{
    return this->wifiIsConnected;
}

ActiveWirelessMode WirelessAPI::getActiveWirelessMode()
{
    return this->activeWirelessMode;
}

void WirelessAPI::ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
    }
}

void WirelessAPI::wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi STA started, trying to connect...");
            esp_wifi_connect();   
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi STA connected to AP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi STA disconnected, reconnecting...");
            esp_wifi_connect();  
            break;
    }
}