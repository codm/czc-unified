#ifndef WIFISTALINK_H_
#define WIFISTALINK_H_

#include <atomic>
#include <cstddef>
#include <mutex>
 
#include "IWirelessLink.h"
#include "IWifiScan.h"
#include "IEventSink.h"
#include "network_event.h"
#include "WifiCreds.h"
#include "WifiDriver.h"
 
#include "esp_event.h"
#include "esp_netif_types.h"
 
/** @brief WiFi station: uplink + scan. Shares the radio with SoftApLink via WifiDriver. */
class WifiStaLink final : public IWirelessLink, public IWifiScan {
public:
    WifiStaLink(WifiDriver& drv, IEventSink& sink) : drv_(drv), sink_(sink) {}
    ~WifiStaLink() override { stop(); }
 
    WifiStaLink(const WifiStaLink&)            = delete;
    WifiStaLink& operator=(const WifiStaLink&) = delete;
 
    // ILink_hal
    esp_err_t start() override;
    void      stop() override;
    LinkId    id() const override    { return LinkId::Sta; }
    LinkState state() const override { return state_.load(); }
    esp_netif_t* netif() const override { return netif_; }
    bool      ipInfo(esp_netif_ip_info_t& out) const override;

    // IWirelessLink
    void      setCredentials(const WifiCreds& creds) override;
    esp_err_t connect() override;
    void      disconnect() override;
 
    // IWifiScan
    esp_err_t   startScan() override;
    ScanState   scanState() const override { return scanState_.load(); }
    std::size_t results(ApInfo* out, std::size_t max) const override;
 
private:
    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    void onDisconnected(uint8_t reason);
    void onScanDone(bool ok);
    void cleanup();
    static FailReason mapReason(uint8_t reason);
 
    static constexpr const char* TAG = "WifiStaLink";
    static constexpr std::size_t maxScanResults = 20;
 
    WifiDriver& drv_;
    IEventSink& sink_;
 
    esp_netif_t*                 netif_      = nullptr;
    esp_event_handler_instance_t wifiEvtInst_ = nullptr;
    esp_event_handler_instance_t ipEvtInst_   = nullptr;
 
    WifiCreds creds_{};
 
    std::atomic<LinkState> state_{LinkState::Stopped};
    std::atomic<bool>      intentional_{false}; 
 
    mutable std::mutex     scanMtx_;
    ApInfo                 results_[maxScanResults]{};
    std::size_t            resultCount_ = 0;
    std::atomic<ScanState> scanState_{ScanState::Idle};
};
 
#endif // WIFISTALINK_H_