#ifndef ETHERNETLINK_H_
#define ETHERNETLINK_H_

#include <atomic>

#include "INetLink.h"
#include "IEventSink.h"
#include "esp_eth_driver.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "esp_netif_types.h"
#include "esp_event.h"  

class EthernetLink final : INetLink
{
private:
    constexpr static const char* TAG = "EthernetLink";

    IEventSink& sink_;

    esp_eth_mac_t* mac_ = nullptr;
    esp_eth_phy_t* phy_ = nullptr;
    esp_eth_handle_t eth_ = nullptr;
    esp_netif_t* netif_ = nullptr;
    esp_eth_netif_glue_handle_t glue_ = nullptr;

    esp_event_handler_instance_t ethEvtInst_ = nullptr;
    esp_event_handler_instance_t ipEvtInst_ = nullptr;

    std::atomic<LinkState> state_{LinkState::Stopped};

    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    void cleanup();

public:
    explicit EthernetLink(IEventSink& sink);

    // copies with same handles not allowed
    EthernetLink(const EthernetLink&) = delete;
    EthernetLink& operator=(const EthernetLink&) = delete;

    esp_err_t start() override;
    void stop() override;
    esp_err_t connect() override; 
    void disconnect() override;
    bool ipInfo(esp_netif_ip_info_t& out) const override;
};

#endif // ETHERNETLINK_H_
