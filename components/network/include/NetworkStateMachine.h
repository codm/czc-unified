#ifndef NVS_H_
#define NVS_H_

#include "esp_log.h"
#include "esp_netif.h"

#include "EthernetAPI.h"
#include "WirelessAPI.h"
#include "nvs_bind.h"
#include "network_event.h"
#include "network_config.h"
#include "esp_timer.h"

enum class NetworkState {
    INIT,
    ETHERNET,
    WIFI,
    ACCESS_POINT,
    RETRY_ETHERNET,
    RETRY_WIFI
};

class NetworkStateMachine
{
private:
    static const char* TAG;
    NetworkState currentState;
    EthernetAPI ethernetAPI;
    WirelessAPI wirelessAPI;
    esp_timer_handle_t initTimer;
    esp_timer_handle_t retryTimer;

    /**
     * @brief ESP event handler callback for `IP_EVENT`. Dispatches to dedicated handler methods.
     *
     * Handles: `IP_EVENT_GOT_IP6`, `IP_EVENT_STA_GOT_IP`, `IP_EVENT_ETH_GOT_IP`,
     *          `IP_EVENT_STA_LOST_IP`, `IP_EVENT_ETH_LOST_IP`
     */
    static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    /**
     * @brief ESP event handler callback for `NETWORK_EVENT`. Dispatches to dedicated handler methods.
     *
     * Handles: `NETWORK_EVENT_CONFIG_UPDATED`, `NETWORK_EVENT_INIT_TIMEOUT`,
     *          `NETWORK_EVENT_WIFI_DISCONNECTED`, `NETWORK_EVENT_RETRY_TIMEOUT`
     *
     * @note Behavior overview:
     *   - Wifi starts only when a config exists in NVS; Ethernet starts unconditionally.
     *   - After 5 s without any connection the AccessPoint is opened.
     *   - On Wifi disconnect a retry timer starts; on timeout it falls back to Ethernet (or
     *     reopens the AccessPoint when no stored config exists).
     *   - A config update tears down all active interfaces and restarts in WIFI mode.
     */
    static void network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    void initAndStartInitTimer();
    static void initTimerCallback(void* args);
    void initRetryTimer();
    static void retryTimerCallback(void* args);
    bool nvsWifiConfigExists();

    // -------------------------------------------------------------------------
    // IP event handlers
    // -------------------------------------------------------------------------

    /** @brief Logs the received IPv6 address. */
    void onGotIpv6(ip_event_got_ip6_t* event_data);

    /**
     * @brief Stops the init timer, marks Wifi as connected, persists the config to NVS
     *        and transitions to `WIFI` state (unless Ethernet is already active).
     */
    void onWifiGotIp();

    /**
     * @brief Stops the init timer, marks Ethernet as connected, transitions to `ETHERNET`
     *        state and creates an IPv6 link-local address on the interface.
     *
     * @param[in] event_data IP event data carrying the netif handle
     */
    void onEthernetGotIp(ip_event_got_ip_t* event_data);

    /** @brief Marks Wifi as disconnected and transitions to `ETHERNET` state. */
    void onWifiLostIp();

    /**
     * @brief Marks Ethernet as disconnected. Transitions to `RETRY_WIFI` if a stored Wifi
     *        config exists (waiting for IP before entering `WIFI`), otherwise opens the AccessPoint.
     */
    void onEthernetLostIp();

    // -------------------------------------------------------------------------
    // Network event handlers
    // -------------------------------------------------------------------------

    /**
     * @brief Tears down the currently active interface (Wifi, AccessPoint or Ethernet)
     *        and transitions to `RETRY_WIFI` so the new config is applied and an IP is
     *        awaited before entering `WIFI`.
     */
    void onWifiConfigUpdated();

    /**
     * @brief Applies the new Ethernet IP configuration from NVS (DHCP or static IP)
     *        and reinitialises the Ethernet interface.
     */
    void onEthernetConfigUpdated();

    /**
     * @brief Fired when the init timer expires. Opens the AccessPoint if no connection
     *        was established during the init window.
     */
    void onInitTimeout();

    /** @brief Transitions from `WIFI` to `RETRY_WIFI` to start the reconnect timer. */
    void onWifiDisconnected();

    /**
     * @brief Fired when the retry timer expires. Always reopens the AccessPoint so the
     *        user can re-enter credentials regardless of what triggered the retry.
     */
    void onRetryTimeout();

    // -------------------------------------------------------------------------

    /**
     * @brief Uninit given NSM state
     *
     * @param[in] currentState State which will be exited
     * @param[in] newState     Next state that will be set
     *
     * @warning `newState` is only needed for the Wifi and AP retry transitions
     *
     * @return `ESP_OK` currently no return values implemented
     */
    esp_err_t exitState(NetworkState currentState, NetworkState newState);

    /**
     * @brief Init new NSM state
     *
     * @param[in] newState State which will be entered
     *
     * @return `ESP_OK` currently no return values implemented
     */
    esp_err_t initNewState(NetworkState newState);
public:
    /**
     * @brief Default Constructor: Creates EthernetAPI & WirelessAPI Objects as well as 
     *        retry Timers, netif setup, etc.
     */
    NetworkStateMachine();
    ~NetworkStateMachine();

    /**
     * @brief Set State of Network State Machine - Uninit previous state and init new state.
     * 
     * @param[in]   newState State you want the NSM to be
     * 
     * @return void
     */
    void setState(NetworkState newState);

    /**
     * @brief Get current State of Network State machine
     * 
     * @return `NetworkState` current State
     */
    NetworkState getState();

    /**
     * @brief Wait until a valid internet connection is available
     *
     *        --> getState == WIFI || getState == Ethernet
     *
     * @warning Blocking!
     *
     * @returns When internet is connected successfully
     */
    void waitUntilInternetIsConnected();

    /**
     * @brief Read the current WiFi configuration from NVS.
     *
     * @param[out] out  Destination struct
     *
     * @return `ESP_OK` on success
     */
    esp_err_t getWifiConfig(wifi_config_data_t* out);

    /**
     * @brief Persist a new WiFi configuration to NVS and post
     *        `NETWORK_EVENT_CONFIG_UPDATED` to trigger reconnection.
     *
     * @param[in] cfg  New configuration to apply
     *
     * @return `ESP_OK` on success
     */
    esp_err_t setWifiConfig(const wifi_config_data_t* cfg);

    /**
     * @brief Read the current Ethernet configuration from NVS.
     *
     * @param[out] out  Destination struct
     *
     * @return `ESP_OK` on success
     */
    esp_err_t getEthernetConfig(ethernet_config_data_t* out);

    /**
     * @brief Persist a new Ethernet configuration to NVS and post
     *        `NETWORK_EVENT_ETH_CONFIG_UPDATED` to trigger reinitialisation.
     *
     * @param[in] cfg  New configuration to apply
     *
     * @return `ESP_OK` on success
     */
    esp_err_t setEthernetConfig(const ethernet_config_data_t* cfg);

    /**
     * @brief Return the current network connection status.
     *
     * @param[out] out  Destination struct
     *
     * @return `ESP_OK` on success
     */
    esp_err_t getNetworkStatus(network_status_t* out);

    /**
     * @brief Set Mdns hostname of ESP32
     * 
     * @param[in] hostname 
     * 
     * @return `ESP_OK` success - `ESP_ERR_INVALID_ARG` Parameter error - `ESP_ERR_NO_MEM` memory error
     */
    esp_err_t setMdnsHostname(const char* hostname);

    /**
     * @brief Fill a `web_network_callbacks_t` struct with static C shims
     *        that forward network config requests to this NetworkStateMachine instance.
     *
     *        Call this before `esp_br_web_start()` in main.
     *
     * @param[out] cbs  Network callback struct to fill
     *
     * @return void
     */
    void fillNetworkCallbacks(web_network_callbacks_t* cbs);
};


#endif /* NVS_H_ */