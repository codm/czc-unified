#include "EthernetLink.h"

#include "board_config.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth_phy_lan87xx.h"

#include "esp_check.h"
#include "esp_log.h"

void EthernetLink::onEvent(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    EthernetLink* self = static_cast<EthernetLink*>(arg);

    if (base == ETH_EVENT) {
        switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            self->state_ = LinkState::Connecting;
            self->sink_.post({.type = EvType::LinkUp, .link = LinkId::Eth});
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            self->state_ = LinkState::Down;
            self->sink_.post({.type = EvType::LinkDown, .link = LinkId::Eth});
            break;
        default: break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_ETH_GOT_IP) {
            auto* ev = static_cast<ip_event_got_ip_t*>(data);
            if (ev->esp_netif != self->netif_) return; // not our interface
            self->state_ = LinkState::Up;
            self->sink_.post({.type = EvType::GotIp, .link = LinkId::Eth});
        } else if (id == IP_EVENT_ETH_LOST_IP) {
            self->state_ = LinkState::Connecting;     // Link up - ip gone
            self->sink_.post({.type = EvType::LostIp, .link = LinkId::Eth});
        }
    }
}

void EthernetLink::cleanup()
{
    if (ipEvtInst_)  { esp_event_handler_instance_unregister(IP_EVENT,  ESP_EVENT_ANY_ID, ipEvtInst_);  ipEvtInst_  = nullptr; }
    if (ethEvtInst_) { esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, ethEvtInst_); ethEvtInst_ = nullptr; }
    if (glue_)  { esp_eth_del_netif_glue(glue_);  glue_  = nullptr; }
    if (eth_)   { esp_eth_driver_uninstall(eth_); eth_   = nullptr; }
    if (netif_) { esp_netif_destroy(netif_);      netif_ = nullptr; }
    if (phy_)   { phy_->del(phy_);                phy_   = nullptr; }
    if (mac_)   { mac_->del(mac_);                mac_   = nullptr; }
}

EthernetLink::EthernetLink(IEventSink &sink)
    : sink_(sink)
{
}

esp_err_t EthernetLink::start()
{
    if (eth_ != nullptr) {
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

    mac_ = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    phy_ = esp_eth_phy_new_lan87xx(&phy_config);
    if (!mac_ || !phy_) { cleanup(); return ESP_FAIL; }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac_, phy_);
    esp_err_t err = esp_eth_driver_install(&config, &eth_);
    if (err != ESP_OK) { cleanup(); return err; }

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.route_prio = 200; // eth higher prio then wifi                  
    esp_netif_config_t cfg = { .base = &base, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH };
    netif_ = esp_netif_new(&cfg);
    esp_netif_set_hostname(netif_, Board::HOSTNAME);
    glue_ = esp_eth_new_netif_glue(eth_);
    esp_netif_attach(netif_, glue_);

    esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                        &EthernetLink::onEvent, this, &ethEvtInst_);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                        &EthernetLink::onEvent, this, &ipEvtInst_);

    err = esp_eth_start(eth_);
    if (err != ESP_OK) { cleanup(); return err; }

    state_ = LinkState::Down;
    ESP_LOGD(TAG, "Successfully started EthernetLink");
    return ESP_OK;

}

void EthernetLink::stop()
{
    if (eth_ == nullptr) return;
    esp_eth_stop(eth_);
    cleanup();
    state_ = LinkState::Stopped;
    ESP_LOGD(TAG, "EthernetLink stopped");
}

esp_err_t EthernetLink::connect()
{
    return eth_ ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void EthernetLink::disconnect()
{
    // passive interface - no manual disconnecting
}

bool EthernetLink::ipInfo(esp_netif_ip_info_t &out) const
{
    if (state_ != LinkState::Up) return false;
    return esp_netif_get_ip_info(netif_, &out) == ESP_OK && out.ip.addr != 0;
}
