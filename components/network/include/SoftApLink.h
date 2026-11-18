
#ifndef SOFTAPLINK_H_
#define SOFTAPLINK_H_

#include <atomic>
#include <cstdint>
 
#include "ILink_hal.h"
#include "WifiDriver.h"

#include "esp_event.h"
#include "esp_netif_types.h"

/** @brief Soft AP for the provisioning portal (DHCP server on 192.168.4.1). Posts no events. */
class SoftApLink final : public ILink_hal {
public:
    explicit SoftApLink(WifiDriver& drv) : drv_(drv) {}
    ~SoftApLink() override { stop(); }

    SoftApLink(const SoftApLink&)            = delete;
    SoftApLink& operator=(const SoftApLink&) = delete;

    // ILink_hal
    esp_err_t start() override;
    void      stop() override;
    LinkId    id() const override    { return LinkId::Ap; }
    LinkState state() const override { return state_.load(); }
    esp_netif_t* netif() const override { return netif_; }
    bool      ipInfo(esp_netif_ip_info_t& out) const override;   // AP IP (192.168.4.1)

    /** @brief Number of currently associated portal clients. */
    uint8_t clientCount() const { return clients_.load(); }
    /** @brief AP SSID (prefix + last 2 MAC bytes); empty before the first start(). */
    const char* ssid() const { return ssid_; }
 
private:
    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    esp_err_t configure();
    void cleanup();
 
    static constexpr const char* TAG = "soft_ap";
    static constexpr uint8_t kChannel        = 1; 
    static constexpr uint8_t kMaxConnections = 4;
 
    WifiDriver& drv_;
 
    esp_netif_t*                 netif_       = nullptr;
    esp_event_handler_instance_t wifiEvtInst_ = nullptr;
 
    char ssid_[33]{};
 
    std::atomic<LinkState> state_{LinkState::Stopped};
    std::atomic<uint8_t>   clients_{0};
};
 
#endif // SOFTAPLINK_H_
