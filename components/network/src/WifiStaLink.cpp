
#include "WifiStaLink.h"

#include <algorithm>
#include <cstring>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "board_config.h"

// ---------------------------------------------------------------- Lifecycle

esp_err_t WifiStaLink::start()
{
    if (netif_ != nullptr) return ESP_ERR_INVALID_STATE;

    netif_ = esp_netif_create_default_wifi_sta();
    if (netif_ == nullptr) {
        ESP_LOGE(TAG, "netif create failed");
        return ESP_FAIL;
    }
    esp_netif_set_hostname(netif_, Board::HOSTNAME);

    esp_err_t err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiStaLink::onEvent, this, &wifiEvtInst_);
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(
            IP_EVENT, ESP_EVENT_ANY_ID, &WifiStaLink::onEvent, this, &ipEvtInst_);
    }
    if (err == ESP_OK) {
        err = drv_.acquire(WifiDriver::Role::Sta);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        cleanup();
        return err;
    }

    state_ = LinkState::Down;
    ESP_LOGD(TAG, "started");
    return ESP_OK;
}

void WifiStaLink::stop()
{
    if (netif_ == nullptr) return;
    cleanup();
    state_     = LinkState::Stopped;
    scanState_ = ScanState::Idle;
    ESP_LOGD(TAG, "stopped");
}

void WifiStaLink::cleanup()
{
    if (ipEvtInst_)   { esp_event_handler_instance_unregister(IP_EVENT,   ESP_EVENT_ANY_ID, ipEvtInst_);   ipEvtInst_   = nullptr; }
    if (wifiEvtInst_) { esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvtInst_); wifiEvtInst_ = nullptr; }
    drv_.release(WifiDriver::Role::Sta);
    if (netif_) { esp_netif_destroy_default_wifi(netif_); netif_ = nullptr; }
    intentional_ = false;
}

// ---------------------------------------------------------------- IWirelessLink

void WifiStaLink::setCredentials(const WifiCreds& creds)
{
    creds_ = creds;
}

esp_err_t WifiStaLink::connect()
{
    if (state_ == LinkState::Stopped) {
        ESP_LOGW(TAG, "connect: link stopped");
        return ESP_ERR_INVALID_STATE;
    }
    if (!creds_.valid()) {
        ESP_LOGW(TAG, "connect: no creds");
        return ESP_ERR_INVALID_ARG;
    }

    if (state_ != LinkState::Down) disconnect();

    wifi_config_t cfg{};
    std::memcpy(cfg.sta.ssid,     creds_.ssid, std::strlen(creds_.ssid));
    std::memcpy(cfg.sta.password, creds_.pass, std::strlen(creds_.pass));
    cfg.sta.threshold.authmode = creds_.open() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable    = true;
    cfg.sta.sae_pwe_h2e        = WPA3_SAE_PWE_BOTH;
    cfg.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method        = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed: %s", esp_err_to_name(err));
        return err;
    }

    state_ = LinkState::Connecting;
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        state_ = LinkState::Down;
        return err;
    }
    ESP_LOGD(TAG, "connecting to '%s' (%s)", creds_.ssid, creds_.open() ? "open" : "secured");
    return ESP_OK;
}

void WifiStaLink::disconnect()
{
    const LinkState s = state_.load();
    if (s != LinkState::Connecting && s != LinkState::Up) return;

    ESP_LOGD(TAG, "disconnect (intentional, was %s)", toString(s));
    intentional_ = true;
    esp_wifi_disconnect();
    state_ = LinkState::Down;
}

bool WifiStaLink::ipInfo(esp_netif_ip_info_t& out) const
{
    if (state_ != LinkState::Up) return false;
    return esp_netif_get_ip_info(netif_, &out) == ESP_OK && out.ip.addr != 0;
}

// ---------------------------------------------------------------- Events (sys_evt task)

void WifiStaLink::onEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* self = static_cast<WifiStaLink*>(arg);

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_CONNECTED: {
            auto* d = static_cast<wifi_event_sta_connected_t*>(data);
            ESP_LOGI(TAG, "associated with '%.*s' ch %u", d->ssid_len, reinterpret_cast<const char*>(d->ssid), d->channel);
            self->sink_.post({.type = EvType::LinkUp, .link = LinkId::Sta});
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* d = static_cast<wifi_event_sta_disconnected_t*>(data);
            self->onDisconnected(d->reason);
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            auto* d = static_cast<wifi_event_sta_scan_done_t*>(data);
            self->onScanDone(d->status == 0);
            break;
        }
        default: break;
        }
    } else if (base == IP_EVENT) {
        switch (id) {
        case IP_EVENT_STA_GOT_IP: {
            auto* ev = static_cast<ip_event_got_ip_t*>(data);
            ESP_LOGI(TAG, "got ip " IPSTR " mask " IPSTR " gw " IPSTR,
                     IP2STR(&ev->ip_info.ip), IP2STR(&ev->ip_info.netmask), IP2STR(&ev->ip_info.gw));
            self->state_ = LinkState::Up;
            self->sink_.post({.type = EvType::GotIp, .link = LinkId::Sta});
            break;
        }
        case IP_EVENT_STA_LOST_IP:
            if (self->state_ == LinkState::Up) {
                ESP_LOGW(TAG, "lost ip");
                self->state_ = LinkState::Connecting;
                self->sink_.post({.type = EvType::LostIp, .link = LinkId::Sta});
            }
            break;
        default: break;
        }
    }
}

void WifiStaLink::onDisconnected(uint8_t reason)
{
    if (reason == WIFI_REASON_ASSOC_LEAVE && intentional_.exchange(false)) {
        ESP_LOGD(TAG, "disconnected (intentional)");
        return;
    }

    const LinkState prev = state_.exchange(LinkState::Down);
    const FailReason fr  = mapReason(reason);

    if (prev == LinkState::Up) {
        ESP_LOGW(TAG, "connection lost, reason %u (%s)", reason, toString(fr));
        sink_.post({.type = EvType::LinkDown, .link = LinkId::Sta, .reason = fr});
    } else if (prev == LinkState::Connecting) {
        ESP_LOGW(TAG, "connect failed, reason %u (%s)", reason, toString(fr));
        sink_.post({.type = EvType::ConnectFailed, .link = LinkId::Sta, .reason = fr});
    } else {
        ESP_LOGD(TAG, "disconnected in state %s, reason %u - ignored", toString(prev), reason);
    }
}

FailReason WifiStaLink::mapReason(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
        return FailReason::AuthFail;
    case WIFI_REASON_NO_AP_FOUND:
        return FailReason::NoAp;
    default:
        return FailReason::Other;
    }
}

// ---------------------------------------------------------------- IWifiScan

esp_err_t WifiStaLink::startScan()
{
    if (state_ == LinkState::Stopped) {
        ESP_LOGW(TAG, "scan: link stopped");
        return ESP_ERR_INVALID_STATE;
    }
    if (scanState_ == ScanState::Running) return ESP_OK;

    wifi_scan_config_t sc{};
    sc.show_hidden = false;

    esp_err_t err = esp_wifi_scan_start(&sc, false);
    if (err == ESP_OK) {
        scanState_ = ScanState::Running;
        ESP_LOGD(TAG, "scan started");
    } else {
        ESP_LOGW(TAG, "scan start rejected: %s", esp_err_to_name(err));   // e.g. busy while connecting
    }
    return err;
}

void WifiStaLink::onScanDone(bool ok)
{
    if (!ok) {
        ESP_LOGW(TAG, "scan failed");
        esp_wifi_clear_ap_list();
        scanState_ = ScanState::Failed;
        return;
    }

    std::lock_guard lock(scanMtx_);
    resultCount_ = 0;

    wifi_ap_record_t rec;
    while (esp_wifi_scan_get_ap_record(&rec) == ESP_OK) {
        const char* ssid = reinterpret_cast<const char*>(rec.ssid);
        if (ssid[0] == '\0') continue;

        auto* end = results_ + resultCount_;
        auto* dup = std::find_if(results_, end,
                                 [&](const ApInfo& a) { return std::strcmp(a.ssid, ssid) == 0; });
        ApInfo* slot = nullptr;
        if (dup != end) {
            if (rec.rssi <= dup->rssi) continue;
            slot = dup;
        } else if (resultCount_ < maxScanResults) {
            slot = &results_[resultCount_++];
        } else {
            auto* weakest = std::min_element(results_, end,
                                             [](const ApInfo& a, const ApInfo& b) { return a.rssi < b.rssi; });
            if (rec.rssi <= weakest->rssi) continue;
            slot = weakest;
        }

        std::strncpy(slot->ssid, ssid, sizeof(slot->ssid) - 1);
        slot->ssid[sizeof(slot->ssid) - 1] = '\0';
        slot->rssi    = rec.rssi;
        slot->secured = rec.authmode != WIFI_AUTH_OPEN;
    }
    esp_wifi_clear_ap_list();

    std::sort(results_, results_ + resultCount_,
              [](const ApInfo& a, const ApInfo& b) { return a.rssi > b.rssi; });
    scanState_ = ScanState::Done;
    ESP_LOGD(TAG, "scan done, %u networks", static_cast<unsigned>(resultCount_));
}

std::size_t WifiStaLink::results(ApInfo* out, std::size_t max) const
{
    std::lock_guard lock(scanMtx_);
    const std::size_t n = std::min(max, resultCount_);
    std::copy_n(results_, n, out);
    return n;
}
