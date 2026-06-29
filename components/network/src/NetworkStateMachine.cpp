#include "NetworkStateMachine.h"
#include "mdns.h"

const char* NetworkStateMachine::TAG = "network-state-machine";
ESP_EVENT_DEFINE_BASE(NETWORK_EVENT);

NetworkStateMachine::NetworkStateMachine()
    : currentState(NetworkState::INIT), ethernetAPI(), wirelessAPI(), initTimer(NULL), retryTimer(NULL)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("codm-otbr"));
    ethernetAPI.initEthernet();
    if(nvsWifiConfigExists()) {
        ESP_LOGW(TAG, "Wifi was previously configured, loading config from NVS and starting wifi");
        wifi_config_data_t cfg{};
        if(NvsBinding::readWifiConfig(cfg) == ESP_OK)
        {
            wirelessAPI.setWirelessConfig(cfg.ssid, cfg.password);
            wirelessAPI.initWifi();
        }
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::ip_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::network_event_handler, this));

    initRetryTimer();
    initAndStartInitTimer();
}

NetworkStateMachine::~NetworkStateMachine()
{
    ethernetAPI.closeEthernet();
    ActiveWirelessMode wirelessMode = wirelessAPI.getActiveWirelessMode();
    if(wirelessMode == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
    else if(wirelessMode == ActiveWirelessMode::ACCESSPOINT) wirelessAPI.closeAccessPoint();
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkStateMachine::ip_event_handler));
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

// -----------------------------------------------------------------------------
// Timer helpers
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// IP event handler + handlers
// -----------------------------------------------------------------------------

void NetworkStateMachine::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto* self = static_cast<NetworkStateMachine*>(arg);
    switch (event_id) {
        case IP_EVENT_GOT_IP6:
            self->onGotIpv6(static_cast<ip_event_got_ip6_t*>(event_data));
            break;
        case IP_EVENT_STA_GOT_IP:
            self->onWifiGotIp();
            break;
        case IP_EVENT_ETH_GOT_IP:
            self->onEthernetGotIp(static_cast<ip_event_got_ip_t*>(event_data));
            break;
        case IP_EVENT_STA_LOST_IP:
            self->onWifiLostIp();
            break;
        case IP_EVENT_ETH_LOST_IP:
            self->onEthernetLostIp();
            break;
    }
}

void NetworkStateMachine::onGotIpv6(ip_event_got_ip6_t* event_data)
{
    ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(event_data->ip6_info.ip));
}

void NetworkStateMachine::onWifiGotIp()
{
    ESP_LOGI(TAG, "Wifi got IP");
    if(initTimer) esp_timer_stop(initTimer);
    wirelessAPI.setWifiIsConnected(true);

    ESP_LOGI(TAG, "Saving Wifi config to NVS...");
    wifi_config_data_t cfg{};
    NvsBinding::readWifiConfig(cfg);  // preserve existing DHCP/IP/DNS settings
    strlcpy(cfg.ssid,     wirelessAPI.getSsid(),     sizeof(cfg.ssid));
    strlcpy(cfg.password, wirelessAPI.getPassword(), sizeof(cfg.password));
    NvsBinding::writeWifiConfig(cfg);
    
    if (!ethernetAPI.getEthIsConnected())
    {
        setState(NetworkState::WIFI);
        ESP_LOGI(TAG, "Wifi got IP -> changed mode to Wifi");
    }
}

void NetworkStateMachine::onEthernetGotIp(ip_event_got_ip_t* event_data)
{
    if(initTimer) esp_timer_stop(initTimer);
    ethernetAPI.setEthIsConnected(true);
    setState(NetworkState::ETHERNET);
    ESP_LOGI(TAG, "Ethernet got IP -> changed mode to Ethernet");
    esp_netif_create_ip6_linklocal(event_data->esp_netif);
    EthernetAPI::logNetDiag(event_data->esp_netif);
}

void NetworkStateMachine::onWifiLostIp()
{
    wirelessAPI.setWifiIsConnected(false);
    setState(NetworkState::ETHERNET);
    ESP_LOGI(TAG, "Wifi lost IP -> changed mode to Ethernet");
}

void NetworkStateMachine::onEthernetLostIp()
{
    ESP_LOGW(TAG, "Ethernet lost IP");
    ethernetAPI.setEthIsConnected(false);
    if(NvsBinding::wifiConfigExists())
    {
        wifi_config_data_t cfg{};
        if(NvsBinding::readWifiConfig(cfg) == ESP_OK)
        {
            wirelessAPI.setWirelessConfig(cfg.ssid, cfg.password);
            setState(NetworkState::RETRY_WIFI);
            ESP_LOGI(TAG, "Ethernet lost IP -> WiFi config found, switching to Wifi");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Ethernet lost IP -> no WiFi config, opening AccessPoint");
        setState(NetworkState::ACCESS_POINT);
    }
}

// -----------------------------------------------------------------------------
// Network event handler + handlers
// -----------------------------------------------------------------------------

void NetworkStateMachine::network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto* self = static_cast<NetworkStateMachine*>(arg);
    switch (event_id) {
        case NETWORK_EVENT_CONFIG_UPDATED:
            self->onWifiConfigUpdated();
            break;
        case NETWORK_EVENT_ETH_CONFIG_UPDATED:
            self->onEthernetConfigUpdated();
            break;
        case NETWORK_EVENT_INIT_TIMEOUT:
            self->onInitTimeout();
            break;
        case NETWORK_EVENT_WIFI_DISCONNECTED:
            self->onWifiDisconnected();
            break;
        case NETWORK_EVENT_RETRY_TIMEOUT:
            self->onRetryTimeout();
            break;
    }
}

void NetworkStateMachine::onWifiConfigUpdated()
{
    ESP_LOGI(TAG, "Wifi config changed -> changing mode to Wifi");
    if(wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
    else if(wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::ACCESSPOINT) wirelessAPI.closeAccessPoint();
    else if(ethernetAPI.getEthIsInitialised()) ethernetAPI.closeEthernet();

    wifi_config_data_t cfg{};
    if (NvsBinding::readWifiConfig(cfg) == ESP_OK) {
        wirelessAPI.setWirelessConfig(cfg.ssid, cfg.password);
    }

    setState(NetworkState::RETRY_WIFI);
}

void NetworkStateMachine::onInitTimeout()
{
    ESP_LOGI(TAG, "Init timer finished");
    if(currentState == NetworkState::INIT) {
        if(wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI)
            wirelessAPI.closeWifi();
        ESP_LOGI(TAG, "Starting Accesspoint!");
        setState(NetworkState::ACCESS_POINT);
    }
    initTimer = NULL;
}

void NetworkStateMachine::onWifiDisconnected()
{
    ESP_LOGW(TAG, "Wifi disconnected -> starting retry timer");
    if(currentState == NetworkState::WIFI)
        setState(NetworkState::RETRY_WIFI);
}

void NetworkStateMachine::onRetryTimeout()
{
    ESP_LOGI(TAG, "Retry timer finished");
    if(currentState == NetworkState::RETRY_WIFI) {
        ESP_LOGW(TAG, "Wifi retry timed out -> opening AccessPoint");
        setState(NetworkState::ACCESS_POINT);
    }
}

// -----------------------------------------------------------------------------
// State machine
// -----------------------------------------------------------------------------

void NetworkStateMachine::setState(NetworkState newState)
{
    if (newState == currentState) return;

    esp_err_t ret = exitState(currentState, newState);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error while exiting NVS State!");
    }

    currentState = newState;

    ret = initNewState(newState);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error while entering new NVS State!");
    }
}

esp_err_t NetworkStateMachine::exitState(NetworkState currentState, NetworkState newState)
{
    switch (currentState) {
        case NetworkState::ACCESS_POINT:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::ACCESSPOINT) wirelessAPI.closeAccessPoint();
            break;
        case NetworkState::WIFI:
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
            if(newState != NetworkState::WIFI) {
                if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::WIFI) wirelessAPI.closeWifi();
            }
            break;
        default:
            break;
    }

    return ESP_OK;
}

esp_err_t NetworkStateMachine::initNewState(NetworkState newState)
{
    switch (currentState) {
        case NetworkState::ACCESS_POINT:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) {
                wirelessAPI.initAccessPoint();
            }
            break;
        case NetworkState::WIFI:
            if(this->wirelessAPI.getActiveWirelessMode() == ActiveWirelessMode::OFF) wirelessAPI.initWifi();
            break;
        case NetworkState::ETHERNET:
            if(this->ethernetAPI.getEthIsInitialised() == false) ethernetAPI.initEthernet();
            break;
        case NetworkState::RETRY_ETHERNET:
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 10000000)); // 10s deadline
            break;
        case NetworkState::RETRY_WIFI:
            if(wirelessAPI.getActiveWirelessMode() != ActiveWirelessMode::WIFI)
                wirelessAPI.initWifi();   // first connect — STA_START triggers esp_wifi_connect()
            else
                wirelessAPI.reconnect(); // already up, just reconnect
            ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, 10000000)); // 10s deadline
            break;
        default:
            break;
    }

    return ESP_OK;
}

NetworkState NetworkStateMachine::getState()
{
    return currentState;
}

void NetworkStateMachine::waitUntilInternetIsConnected()
{
    while (getState() != NetworkState::WIFI &&
           getState() != NetworkState::ETHERNET) 
    {
        vTaskDelay(100);
    }
}

bool NetworkStateMachine::nvsWifiConfigExists()
{
    return NvsBinding::wifiConfigExists();
}

void NetworkStateMachine::onEthernetConfigUpdated()
{
    ESP_LOGI(TAG, "Ethernet config changed — reinitializing interface");
    if(ethernetAPI.getEthIsInitialised())
    {
        ethernetAPI.closeEthernet();
    }

    ethernet_config_data_t cfg{};
    NvsBinding::readEthernetConfig(cfg);

    esp_netif_t* netif{esp_netif_get_handle_from_ifkey("ETH_DEF")};
    if (netif)
    {
        if (cfg.dhcp)
        {
            esp_netif_dhcpc_start(netif);
        }
        else
        {
            esp_netif_dhcpc_stop(netif);
            esp_netif_ip_info_t info{};
            ipaddr_aton(cfg.static_ip, (ip_addr_t*)&info.ip);
            ipaddr_aton(cfg.gateway,   (ip_addr_t*)&info.gw);
            ip4_addr_set_u32(&info.netmask, PP_HTONL(0xFFFFFF00UL));  // /24 default
            esp_netif_set_ip_info(netif, &info);
        }
    }

    ethernetAPI.initEthernet();
    setState(NetworkState::INIT);
    initAndStartInitTimer();
}

// -----------------------------------------------------------------------------
// Network config public API
// -----------------------------------------------------------------------------

esp_err_t NetworkStateMachine::getWifiConfig(wifi_config_data_t* out)
{
    return NvsBinding::readWifiConfig(*out);
}

esp_err_t NetworkStateMachine::setWifiConfig(const wifi_config_data_t* cfg)
{
    esp_err_t ret{NvsBinding::writeWifiConfig(*cfg)};
    if (ret == ESP_OK)
    {
        esp_event_post(NETWORK_EVENT, NETWORK_EVENT_CONFIG_UPDATED, nullptr, 0, portMAX_DELAY);
    }
    return ret;
}

esp_err_t NetworkStateMachine::getEthernetConfig(ethernet_config_data_t* out)
{
    return NvsBinding::readEthernetConfig(*out);
}

esp_err_t NetworkStateMachine::setEthernetConfig(const ethernet_config_data_t* cfg)
{
    esp_err_t ret{NvsBinding::writeEthernetConfig(*cfg)};
    if (ret == ESP_OK)
    {
        esp_event_post(NETWORK_EVENT, NETWORK_EVENT_ETH_CONFIG_UPDATED, nullptr, 0, portMAX_DELAY);
    }
    return ret;
}

esp_err_t NetworkStateMachine::getNetworkStatus(network_status_t* out)
{
    memset(out, 0, sizeof(*out));
    out->mode      = static_cast<int>(currentState);
    out->connected = (currentState == NetworkState::ETHERNET ||
                      currentState == NetworkState::WIFI);

    if (out->connected)
    {
        const char* ifkey{currentState == NetworkState::ETHERNET ? "ETH_DEF" : "WIFI_STA_DEF"};
        esp_netif_t* netif{esp_netif_get_handle_from_ifkey(ifkey)};
        if (netif)
        {
            esp_netif_ip_info_t info{};
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK)
            {
                snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&info.ip));
            }
        }
    }
    return ESP_OK;
}

void NetworkStateMachine::fillNetworkCallbacks(web_network_callbacks_t* cbs)
{
    cbs->get_wifi_config = [](void* ctx, wifi_config_data_t* out)
    {
        return static_cast<NetworkStateMachine*>(ctx)->getWifiConfig(out);
    };
    cbs->set_wifi_config = [](void* ctx, const wifi_config_data_t* cfg)
    {
        return static_cast<NetworkStateMachine*>(ctx)->setWifiConfig(cfg);
    };
    cbs->get_ethernet_config = [](void* ctx, ethernet_config_data_t* out)
    {
        return static_cast<NetworkStateMachine*>(ctx)->getEthernetConfig(out);
    };
    cbs->set_ethernet_config = [](void* ctx, const ethernet_config_data_t* cfg)
    {
        return static_cast<NetworkStateMachine*>(ctx)->setEthernetConfig(cfg);
    };
    cbs->get_network_status = [](void* ctx, network_status_t* out)
    {
        return static_cast<NetworkStateMachine*>(ctx)->getNetworkStatus(out);
    };
    cbs->ctx = this;
}
