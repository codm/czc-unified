#include "EthernetAPI.h"

const char* EthernetAPI::TAG = "ethernet"; 

EthernetAPI::EthernetAPI()
    : ethConfig(), eth_handle(NULL), ethIsInitialised(false), ethIsConnected(false)
{
}

EthernetAPI::~EthernetAPI()
{
}

void EthernetAPI::initEthernet()
{
    if(eth_handle != nullptr) {
        ESP_LOGW(TAG, "Ethernet was allready initialised!");
        return;
    }
    
    // mac config
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();    
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG(); 
    esp32_emac_config.clock_config.rmii.clock_gpio = ethConfig.clkGpio;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config); 

    // phy config
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = ethConfig.phyAddr;
    phy_config.reset_gpio_num = ethConfig.powerPin;
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    // eth driver config
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy); 
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle)); 

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetAPI::eth_event_handler, NULL));

    // ip stack config
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH(); 
    esp_netif_t *eth_netif = esp_netif_new(&cfg); 

    esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)); 
    esp_eth_start(eth_handle); 
    ethIsInitialised = true;
}

void EthernetAPI::closeEthernet()
{
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetAPI::eth_event_handler);
    esp_eth_stop(eth_handle);
    esp_eth_driver_uninstall(eth_handle); 
    ethIsInitialised = false;
}

bool EthernetAPI::getEthIsInitialised()
{
    return this->ethIsInitialised;
}

void EthernetAPI::setEthIsConnected(bool _ethIsConnected)
{
    this->ethIsConnected = _ethIsConnected;
}

bool EthernetAPI::getEthIsConnected()
{
    return this->ethIsConnected;
}

void EthernetAPI::eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}
