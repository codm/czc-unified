#pragma once
#include "esp_log.h"

#include "EthernetAPI.h"
#include "WirelessAPI.h"
// #include "NvsAPI.h"
#include "network_event.h"
#include "esp_timer.h"

enum class NetworkState {
    INIT,
    ETHERNET,
    WLAN,
    ACCESS_POINT,
    RETRY_ETHERNET,
    RETRY_WIFI
};

class NetworkStateMachine
{
private:
    static const char* TAG;
    NetworkState currentState;
    EthernetAPI& ethernetAPI;
    WirelessAPI& wirelessAPI;
    // NvsAPI& nvsAPI;
    
    // NetworkConfig networkConfig;
    esp_timer_handle_t initTimer;
    esp_timer_handle_t retryTimer;

    static void network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    void initAndStartInitTimer();
    static void initTimerCallback(void* args);
    void initRetryTimer();
    static void retryTimerCallback(void* args);
public:
    NetworkStateMachine(EthernetAPI& _ethernetAPI, WirelessAPI& _wirelessAPI);
    ~NetworkStateMachine();
    void initNetworkStateMachine();
    void closeNetworkStateMachine();
    void setState(NetworkState newState);
};
