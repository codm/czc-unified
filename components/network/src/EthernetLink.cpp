#include "EthernetLink.h"

#include "board_config.h"

#include "esp_event.h"
#include "esp_eth_mac.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy_lan87xx.h"

#include "esp_check.h"
#include "esp_log.h"

void EthernetLink::onEvent(void *arg, esp_event_base_t base, int32_t id, void *data)
{
}

EthernetLink::EthernetLink(IEventSink &sink)
    : sink(sink)
{
}

esp_err_t EthernetLink::start()
{
    if (eth_handle != nullptr) {
        ESP_LOGW(TAG, "Ethernet driver already initialised - aborting start");
        return ESP_ERR_INVALID_STATE;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.clock_config.rmii.clock_gpio = Board::CLK_GPIO;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = Board::PHY_ADDR;
    phy_config.reset_gpio_num = Board::POWER_PIN;
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

    // -- 

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    netif_handle = esp_netif_new(&cfg);
    esp_netif_set_hostname(netif_handle, Board::HOSTNAME);
    esp_netif_attach(netif_handle, esp_eth_new_netif_glue(eth_handle));    
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetLink::onEvent, this));
    esp_eth_start(eth_handle);
    
    ESP_LOGD(TAG, "Ethernet successfully setup!");
    return ESP_OK;
}

void EthernetLink::stop()
{
    ESP_LOGD(TAG, "Stopping EthernetLink");
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetLink::onEvent);
    esp_eth_stop(eth_handle);
    esp_eth_driver_uninstall(eth_handle);
    esp_netif_destroy(netif_handle);
    netif_handle = nullptr;
    eth_handle = nullptr;
}

esp_err_t EthernetLink::connect()
{
    return esp_err_t();
}

void EthernetLink::disconnect()
{
}

bool EthernetLink::ipInfo(esp_netif_ip_info_t &out) const
{
    return false;
}

void EthernetLink::setDefaultRoute(bool primary)
{
}
