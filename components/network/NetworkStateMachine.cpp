#include "NetworkStateMachine.h"

const char* NetworkStateMachine::TAG = "network-state-machine";
ESP_EVENT_DEFINE_BASE(NETWORK_EVENT);

NetworkStateMachine::NetworkStateMachine(EthernetAPI& _ethernetAPI, WirelessAPI& _wirelessAPI) 
    : currentState(NetworkState::INIT), ethernetAPI(_ethernetAPI), wirelessAPI(_wirelessAPI), initTimer(NULL), retryTimer(NULL)
{   
}

NetworkStateMachine::~NetworkStateMachine()
{
}

void NetworkStateMachine::initNetworkStateMachine()
{
    ethernetAPI.initEthernet();
    ESP_ERROR_CHECK(esp_netif_init());
    // if(networkConfig.wifiConfigured) {
    //     ESP_LOGW(TAG, "Wifi was previously configured, loading config from NVS and starting wifi");
    //     wirelessAPI.setWirelessConfig(networkConfig.ssid, networkConfig.password); // if wifi was previously configured, configure accordingly and start wifi 
    //     wirelessAPI.initWifi();
    // }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler, this)); // Ip handler for Ethernet and Wireless
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler, this));

    initAndStartInitTimer();
}

void NetworkStateMachine::closeNetworkStateMachine()
{
    ethernetAPI.closeEthernet();
    ActiveWirelessMode wirelessMode = wirelessAPI.getActiveWirelessMode();
    if(wirelessMode == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
    else if(wirelessMode == ActiveWirelessMode::ACCESSPOINT) wirelessAPI.closeAccessPoint();
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(NETWORK_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler));
    esp_timer_delete(retryTimer);
}

void NetworkStateMachine::initAndStartInitTimer()
{
    esp_timer_create_args_t initTimerConfig = {
        .callback = NetworkStateMachine::initTimerCallback,
        .arg = nullptr,
        .name = "networkInitTimer",
        .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&initTimerConfig, &initTimer));
    ESP_ERROR_CHECK(esp_timer_start_once(initTimer, 5000000)); 
}

void NetworkStateMachine::initTimerCallback(void *args)
{
    esp_event_post(NETWORK_EVENT, NETWORK_EVENT_INIT_TIMEOUT, nullptr, 0, portMAX_DELAY);
}

void NetworkStateMachine::initRetryTimer()
{
    esp_timer_create_args_t retryTimerConfig = {
        .callback = NetworkStateMachine::retryTimerCallback,
        .arg = nullptr,
        .name = "retryTimer",
        .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&retryTimerConfig, &retryTimer));
}

void NetworkStateMachine::retryTimerCallback(void *args)
{
    esp_event_post(NETWORK_EVENT, NETWORK_EVENT_RETRY_TIMEOUT, nullptr, 0, portMAX_DELAY);
}

// Behavior: Wifi is started when there is a config in the nvs, eth is started all the time
// After 5s if neither ethernet nor wifi is connected start the accesspoint
// If wifi loses connection start the retry timer and if it finishes without a reestablished connection switch to ethernet
// This is implemented for ethernet -> wifi and wifi -> ethernet 
// When the wifi config is changed the accesspoint, wifi and ethernet shuts down, the config is saved and wifi starts again. 
// Note: When you configure your wifi in the Accesspoint and your credentials are wrong you are cooked
void NetworkStateMachine::network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto* self = static_cast<NetworkStateMachine*>(arg);

    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                self->wirelessAPI.setWifiIsConnected(true);
                ESP_LOGI(TAG, "Wifi got IP"); 
                if(self->ethernetAPI.getEthIsConnected() == false) {
                    self->setState(NetworkState::WLAN);
                    ESP_LOGI(TAG, "Wifi got IP -> changed mode to Wifi");
                }
                break;

            case IP_EVENT_ETH_GOT_IP:
                self->ethernetAPI.setEthIsConnected(true);
                self->setState(NetworkState::ETHERNET);
                ESP_LOGI(TAG, "Ethernet got IP -> changed mode to Ethernet");
                break;

            case IP_EVENT_STA_LOST_IP:
                self->wirelessAPI.setWifiIsConnected(false);
                self->setState(NetworkState::ETHERNET);
                ESP_LOGI(TAG, "Wifi lost IP -> changed mode to Ethernet");
                break;

            case IP_EVENT_ETH_LOST_IP:
                // ESP_LOGI(TAG, "Ethernet lost IP");
                // if(self->networkConfig.wifiConfigured) {
                //     self->ethernetAPI.setEthIsConnected(false);
                //     self->setState(NetworkState::WLAN);
                //     ESP_LOGI(TAG, "Ethernet lost IP -> changed mode to Wifi");
                // } 
                break;
        }
    }
    else if (event_base == NETWORK_EVENT) {
        switch (event_id) {
            case NETWORK_EVENT_CONFIG_UPDATED: 
                ESP_LOGI(TAG, "Wifi config changed -> changing mode to Wifi");
                if(self->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) self->wirelessAPI.closeWifi();
                else if(self->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::ACCESSPOINT) self->wirelessAPI.closeAccessPoint();
                else if(self->ethernetAPI.getEthIsInitialised()) self->ethernetAPI.closeEthernet();
                // self->nvsAPI.getNetworkConfigFromNvs(self->networkConfig);
                // self->wirelessAPI.setWirelessConfig(self->networkConfig.ssid, self->networkConfig.password);
                self->setState(NetworkState::WLAN);
                break;

            case NETWORK_EVENT_INIT_TIMEOUT: 
                ESP_LOGI(TAG, "Init timer finished");
                if(self->currentState == NetworkState::INIT) {
                    ESP_LOGI(TAG, "Starting Accesspoint!");
                    self->setState(NetworkState::ACCESS_POINT);
                }
                esp_timer_delete(self->initTimer);
                break;
        }
    }
}

void NetworkStateMachine::setState(NetworkState newState)
{
    if(newState == currentState) return;

    // state exit 
    switch (currentState) {
        case NetworkState::ACCESS_POINT:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::ACCESSPOINT) wirelessAPI.closeAccessPoint();
            break;
        case NetworkState::WLAN:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
            break;
        case NetworkState::ETHERNET:
            if(this->ethernetAPI.getEthIsInitialised()) ethernetAPI.closeEthernet();
            break;
        case NetworkState::RETRY_ETHERNET: 
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 250000));
            break;
        case NetworkState::RETRY_WIFI: 
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 250000)); 
            break;
        default:
            break;
    }

    currentState = newState;

    // state init
    switch (currentState) {
        case NetworkState::ACCESS_POINT:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) wirelessAPI.initAccessPoint(); // evade double init at first initialisation 
            break;
        case NetworkState::WLAN:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) wirelessAPI.initWifi();
            break;
        case NetworkState::ETHERNET:
            if(this->ethernetAPI.getEthIsInitialised() == false) ethernetAPI.initEthernet();
            break;
        case NetworkState::RETRY_ETHERNET: 
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 250000)); 
            break;
        case NetworkState::RETRY_WIFI: 
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 250000)); 
            break;
        default:
            break;
    }
}