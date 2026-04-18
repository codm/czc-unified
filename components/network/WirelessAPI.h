#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"

enum class ActiveWirelessMode {
    OFF,
    WIFI,
    ACCESSPOINT
};

// Wifi and accesspoint not possible at the same time to prevent undefined behavior
class WirelessAPI
{
private:
    static const char* TAG;
    char* ssid;
    char* password;

    char* accessPointSsid;
    char* accessPointPassword;
    uint8_t accessPointMaxConnected;

    ActiveWirelessMode activeWirelessMode; 
    bool wifiIsConnected = false;

    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
public:
    WirelessAPI(char *_accessPointSsid, char *_accessPointPassword, uint8_t _accessPointMaxConnected);
    ~WirelessAPI();
    void setWirelessConfig(char* _ssid, char* _password);
    void initAccessPoint();
    void closeAccessPoint();
    void initWifi();
    void closeWifi();
    void setWifiIsConnected(bool _wifiIsConnected);
    bool getWifiIsConnected();
    ActiveWirelessMode getActiveWirelessMode();
};

