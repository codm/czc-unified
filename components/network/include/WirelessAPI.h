#ifndef WIRELESSAPI_H_
#define WIRELESSAPI_H_

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_br_wifi_config.h"
#include "network_event.h"

enum class ActiveWirelessMode {
    OFF,
    WIFI,
    ACCESSPOINT
};

struct SoftapConfig {
    char ssid[32] = "otbr-codm";
    char password[64] = "codmcodm";
    uint8_t maxConnected = 10;
};

// Wifi and AccessPoint cannot be active at the same time.
class WirelessAPI
{
private:
    static const char* TAG;
    char ssid[32];
    char password[64];
    const SoftapConfig softapConfig;
    ActiveWirelessMode activeWirelessMode;
    bool wifiIsConnected = false;
    esp_netif_t* wifiNetif = nullptr;
    esp_netif_t* apNetif = nullptr;

    // -------------------------------------------------------------------------
    // Wifi (STA) event handler + handlers
    // -------------------------------------------------------------------------

    /**
     * @brief ESP event handler for `WIFI_EVENT` in STA mode. Dispatches to dedicated handler methods.
     *
     * Handles: `WIFI_EVENT_STA_START`, `WIFI_EVENT_STA_CONNECTED`, `WIFI_EVENT_STA_DISCONNECTED`
     */
    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    /** @brief Triggers `esp_wifi_connect()` once the STA interface is up. */
    void onStaStart();

    /** @brief Logs successful association with the AP. */
    void onStaConnected();

    /**
     * @brief Posts `NETWORK_EVENT_WIFI_DISCONNECTED` to notify the state machine
     *        and trigger the retry logic.
     */
    void onStaDisconnected();

    // -------------------------------------------------------------------------
    // AccessPoint event handler + handlers
    // -------------------------------------------------------------------------

    /**
     * @brief ESP event handler for `WIFI_EVENT` in AP mode. Dispatches to dedicated handler methods.
     *
     * Handles: `WIFI_EVENT_AP_STACONNECTED`, `WIFI_EVENT_AP_STADISCONNECTED`
     */
    static void ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    /**
     * @brief Logs a station joining the AccessPoint.
     *
     * @param[in] event_data Event data containing the station's MAC address and AID
     */
    void onApStaConnected(wifi_event_ap_staconnected_t* event_data);

    /**
     * @brief Logs a station leaving the AccessPoint.
     *
     * @param[in] event_data Event data containing the station's MAC address, AID and disconnect reason
     */
    void onApStaDisconnected(wifi_event_ap_stadisconnected_t* event_data);

public:
    /**
     * @brief Default constructor. Initialises all fields; does not start any interface.
     */
    WirelessAPI();
    ~WirelessAPI();

    /**
     * @brief Set the SSID and password used for the next Wifi connection attempt.
     *
     * @param[in] ssid     Null-terminated SSID (max 31 chars)
     * @param[in] password Null-terminated password (max 63 chars)
     */
    void setWirelessConfig(const char* ssid, const char* password);

    /**
     * @brief Initialise and start the SoftAP and the wifi-config webserver.
     */
    void initAccessPoint();

    /**
     * @brief Stop the SoftAP, deregister the event handler and stop the wifi-config webserver.
     */
    void closeAccessPoint();

    /**
     * @brief Spawn a FreeRTOS task that blocks on `esp_br_wifi_config_get_configured_wifi`
     *        until the user submits credentials via the config webserver.
     *        On success the task posts `NETWORK_EVENT_CONFIG_UPDATED` and terminates.
     */
    void startConfigPollingTask();

    /**
     * @brief Initialise and start Wifi in STA mode with the currently stored credentials.
     */
    void initWifi();

    /**
     * @brief Stop Wifi STA, deregister the event handler and deinit the wifi driver.
     */
    void closeWifi();

    /** @brief Trigger a reconnect attempt on the STA interface. */
    void reconnect();

    /** @return Currently configured SSID. */
    const char* getSsid();

    /** @return Currently configured password. */
    const char* getPassword();

    /**
     * @brief Update the internal connected state.
     *
     * @note Called by the state machine in response to `IP_EVENT_STA_GOT_IP` /
     *       `IP_EVENT_STA_LOST_IP` — not from within this class.
     *
     * @param[in] connected True when a valid IP address has been obtained
     */
    void setWifiIsConnected(bool connected);

    /** @return True if the STA interface currently holds a valid IP address. */
    bool getWifiIsConnected();

    /** @return The currently active wireless mode (`OFF` / `WIFI` / `ACCESSPOINT`). */
    ActiveWirelessMode getActiveWirelessMode();
};

#endif /* WIRELESSAPI_H_ */
