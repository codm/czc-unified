#include "EthernetAPI.h"
#include "esp_eth_phy_lan87xx.h"

const char* EthernetAPI::TAG = "ethernet";

EthernetAPI::EthernetAPI()
    : ethConfig(), eth_handle(NULL), ethNetif(nullptr), ethNetifGlue(nullptr),
      ethIsInitialised(false), ethIsConnected(false)
{
}

EthernetAPI::~EthernetAPI()
{
}

void EthernetAPI::initEthernet()
{
    if(eth_handle != nullptr) {
        ESP_LOGW(TAG, "Ethernet was already initialised!");
        return;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.clock_config.rmii.clock_gpio = ethConfig.clkGpio;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = ethConfig.phyAddr;
    phy_config.reset_gpio_num = ethConfig.powerPin;
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetAPI::eth_event_handler, this));

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    ethNetif = esp_netif_new(&cfg);
    esp_netif_set_hostname(ethNetif, "codm-otbr");

    ethNetifGlue = esp_eth_new_netif_glue(eth_handle);
    esp_netif_attach(ethNetif, ethNetifGlue);
    esp_eth_start(eth_handle);
    ethIsInitialised = true;
}

void EthernetAPI::closeEthernet()
{
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetAPI::eth_event_handler);
    esp_eth_stop(eth_handle);
    esp_eth_del_netif_glue(ethNetifGlue);
    esp_eth_driver_uninstall(eth_handle);
    esp_netif_destroy(ethNetif);
    eth_handle = NULL;
    ethNetif = nullptr;
    ethNetifGlue = nullptr;
    ethIsInitialised = false;
    ethIsConnected = false;
}

// -----------------------------------------------------------------------------
// Ethernet event handler + handlers
// -----------------------------------------------------------------------------

void EthernetAPI::eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto* self = static_cast<EthernetAPI*>(arg);
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:    self->onEthConnected();    break;
        case ETHERNET_EVENT_DISCONNECTED: self->onEthDisconnected(); break;
        case ETHERNET_EVENT_START:        self->onEthStart();        break;
        case ETHERNET_EVENT_STOP:         self->onEthStop();         break;
        default: break;
    }
}

void EthernetAPI::onEthConnected()
{
    uint8_t mac_addr[6] = {0};
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_LOGI(TAG, "Ethernet Link Up");
    ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void EthernetAPI::onEthDisconnected()
{
    ESP_LOGI(TAG, "Ethernet Link Down");
}

void EthernetAPI::onEthStart()
{
    ESP_LOGI(TAG, "Ethernet Started");
}

void EthernetAPI::onEthStop()
{
    ESP_LOGI(TAG, "Ethernet Stopped");
}

// -----------------------------------------------------------------------------
// Getters / Setters
// -----------------------------------------------------------------------------

void EthernetAPI::setEthIsConnected(bool connected)
{
    this->ethIsConnected = connected;
}

bool EthernetAPI::getEthIsInitialised()
{
    return this->ethIsInitialised;
}

void EthernetAPI::getCurrentIp(char* out, size_t out_size)
{
    out[0] = '\0';

    if (ethNetif)
    {
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(ethNetif, &info) == ESP_OK)
        {
            snprintf(out, out_size, IPSTR, IP2STR(&info.ip));
        }
    }
}

bool EthernetAPI::getEthIsConnected()
{
    return this->ethIsConnected;
}
