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
    ESP_ERROR_CHECK(esp_netif_init());
    ethernetAPI.initEthernet();
    if(nvsWifiConfigExists()) {
        ESP_LOGW(TAG, "Wifi was previously configured, loading config from NVS and starting wifi");
        NetworkConfig config;
        if(NvsBinding::readNetworkConfig(config) == ESP_OK)
        {
            wirelessAPI.setWirelessConfig(config.ssid, config.password);
            wirelessAPI.initWifi();
        }
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler, this));

    initRetryTimer();
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
    if(initTimer) {
        esp_timer_stop(initTimer);
        esp_timer_delete(initTimer);
        initTimer = NULL;
    }
    if(retryTimer) {
        esp_timer_stop(retryTimer);
        esp_timer_delete(retryTimer);
        retryTimer = NULL;
    }
}

void NetworkStateMachine::initAndStartInitTimer()
{
    esp_timer_create_args_t initTimerConfig = {
        .callback = NetworkStateMachine::initTimerCallback,
        .arg = this,
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
        .arg = this,
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
            case IP_EVENT_GOT_IP6: {
                auto *ip6_event = static_cast<ip_event_got_ip6_t *>(event_data);
                ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(ip6_event->ip6_info.ip));
                break;
            }

            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "Wifi got IP");
                if(self->initTimer) esp_timer_stop(self->initTimer);
                self->wirelessAPI.setWifiIsConnected(true);
                ESP_LOGI(TAG, "Saving Wifi config to NVS...");
                {
                    NetworkConfig config;
                    strncpy(config.ssid, self->wirelessAPI.getSsid(), sizeof(config.ssid) - 1);
                    config.ssid[sizeof(config.ssid) - 1] = '\0';
                    strncpy(config.password, self->wirelessAPI.getPassword(), sizeof(config.password) - 1);
                    config.password[sizeof(config.password) - 1] = '\0';
                    config.wifiConfigured = true;
                    NvsBinding::writeNetworkConfig(config);
                }
                if(self->ethernetAPI.getEthIsConnected() == false) {
                    self->setState(NetworkState::WLAN);
                    ESP_LOGI(TAG, "Wifi got IP -> changed mode to Wifi");
                }
                break;

            case IP_EVENT_ETH_GOT_IP: {
                if(self->initTimer) esp_timer_stop(self->initTimer);
                self->ethernetAPI.setEthIsConnected(true);
                self->setState(NetworkState::ETHERNET);
                ESP_LOGI(TAG, "Ethernet got IP -> changed mode to Ethernet");
                auto *ip_event = static_cast<ip_event_got_ip_t *>(event_data);
                esp_netif_create_ip6_linklocal(ip_event->esp_netif);
                EthernetAPI::logNetDiag(ip_event->esp_netif);
                break;
            }

            case IP_EVENT_STA_LOST_IP:
                self->wirelessAPI.setWifiIsConnected(false);
                self->setState(NetworkState::ETHERNET);
                ESP_LOGI(TAG, "Wifi lost IP -> changed mode to Ethernet");
                break;

            case IP_EVENT_ETH_LOST_IP:
                ESP_LOGW(TAG, "Ethernet lost IP");
                self->ethernetAPI.setEthIsConnected(false);
                if(NvsBinding::networkConfigExists())
                {
                    NetworkConfig config;
                    if(NvsBinding::readNetworkConfig(config) == ESP_OK)
                    {
                        self->wirelessAPI.setWirelessConfig(config.ssid, config.password);
                        self->setState(NetworkState::WLAN);
                        ESP_LOGI(TAG, "Ethernet lost IP -> WiFi config found, switching to Wifi");
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "Ethernet lost IP -> no WiFi config, opening AccessPoint");
                    self->setState(NetworkState::ACCESS_POINT);
                }
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
                    if(self->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI)
                        self->wirelessAPI.closeWifi();
                    ESP_LOGI(TAG, "Starting Accesspoint!");
                    self->setState(NetworkState::ACCESS_POINT);
                }
                self->initTimer = NULL;
                break;

            case NETWORK_EVENT_WIFI_DISCONNECTED:
                ESP_LOGW(TAG, "Wifi disconnected -> starting retry timer");
                if(self->currentState == NetworkState::WLAN)
                    self->setState(NetworkState::RETRY_WIFI);
                break;

            case NETWORK_EVENT_RETRY_TIMEOUT:
                ESP_LOGI(TAG, "Retry timer finished");
                if(self->currentState == NetworkState::RETRY_WIFI) {
                    if(!self->nvsWifiConfigExists()) {
                        ESP_LOGW(TAG, "First connection attempt timed out, no stored config -> reopening AccessPoint");
                        self->setState(NetworkState::ACCESS_POINT);
                    } else {
                        ESP_LOGW(TAG, "Wifi retry timed out -> falling back to Ethernet");
                        self->setState(NetworkState::ETHERNET);
                    }
                }
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
            // keep WiFi running when transitioning to retry – we want to keep reconnecting
            if(newState != NetworkState::RETRY_WIFI) {
                if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
            }
            break;
        case NetworkState::ETHERNET:
            if(this->ethernetAPI.getEthIsInitialised()) ethernetAPI.closeEthernet();
            break;
        case NetworkState::RETRY_ETHERNET:
            if(esp_timer_is_active(retryTimer)) esp_timer_stop(retryTimer);
            break;
        case NetworkState::RETRY_WIFI:
            if(esp_timer_is_active(retryTimer)) esp_timer_stop(retryTimer);
            // keep WiFi running when reconnect succeeded, close it for all other transitions
            if(newState != NetworkState::WLAN) {
                if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
            }
            break;
        default:
            break;
    }

    currentState = newState;

    // state init
    switch (currentState) {
        case NetworkState::ACCESS_POINT:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) {
                wirelessAPI.initAccessPoint(); // evade double init at first initialisation 
                wirelessAPI.apWaitUntilConnected();
            }
            break;
        case NetworkState::WLAN:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) wirelessAPI.initWifi();
            break;
        case NetworkState::ETHERNET:
            if(this->ethernetAPI.getEthIsInitialised() == false) ethernetAPI.initEthernet();
            break;
        case NetworkState::RETRY_ETHERNET:
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 10000000)); // 10s deadline
            break;
        case NetworkState::RETRY_WIFI:
            wirelessAPI.reconnect();
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 10000000)); // 10s deadline
            break;
        default:
            break;
    }
}

NetworkState NetworkStateMachine::getState()
{
    return currentState;
}

bool NetworkStateMachine::nvsWifiConfigExists()
{
    return NvsBinding::networkConfigExists();
}
