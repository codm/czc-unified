#ifndef ETHERNETLINK_H_
#define ETHERNETLINK_H_

#include "INetLink.h"
#include "IEventSink.h"
#include "esp_eth_driver.h"

class EthernetLink final : INetLink
{
private:
    constexpr static const char* TAG = "EthernetLink";

    IEventSink& sink;
    esp_eth_handle_t eth_handle = nullptr;
    esp_netif_t* netif_handle = nullptr;
    LinkState state = LinkState::Stopped;

    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
public:
    explicit EthernetLink(IEventSink& sink);
    esp_err_t start() override;
    void stop() override;
    esp_err_t connect() override; 
    void disconnect() override;
    bool ipInfo(esp_netif_ip_info_t& out) const override;
    void setDefaultRoute(bool primary) override;
};

#endif // ETHERNETLINK_H_
