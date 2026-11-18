
#include "SoftApLink.h"

#include <cstdio>
#include <cstring>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "board_config.h"

// ---------------------------------------------------------------- Lifecycle

esp_err_t SoftApLink::start()
{
    if (netif_ != nullptr) return ESP_ERR_INVALID_STATE;

    netif_ = esp_netif_create_default_wifi_ap();   // DHCP server, 192.168.4.1
    if (netif_ == nullptr) {
        ESP_LOGE(TAG, "netif create failed");
        return ESP_FAIL;
    }

    esp_err_t err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &SoftApLink::onEvent, this, &wifiEvtInst_);

    if (err == ESP_OK) err = drv_.acquire(WifiDriver::Role::Ap);
    if (err == ESP_OK) err = configure();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        cleanup();
        return err;
    }

    state_ = LinkState::Connecting;
    ESP_LOGI(TAG, "Soft AP '%s' starting (%s, ch %u)", ssid_,
             Board::AP_PASSWORD[0] == '\0' ? "open" : "WPA2", kChannel);
    return ESP_OK;
}

void SoftApLink::stop()
{
    if (netif_ == nullptr) return;
    cleanup();
    state_   = LinkState::Stopped;
    clients_ = 0;
    ESP_LOGI(TAG, "Soft AP stopped");
}

void SoftApLink::cleanup()
{
    if (wifiEvtInst_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvtInst_);
        wifiEvtInst_ = nullptr;
    }
    drv_.release(WifiDriver::Role::Ap);
    if (netif_) { esp_netif_destroy_default_wifi(netif_); netif_ = nullptr; }
}

esp_err_t SoftApLink::configure()
{
    // SSID: prefix + last two MAC bytes, e.g. "CZC-A1B2"
    uint8_t mac[6]{};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (err != ESP_OK) ESP_LOGW(TAG, "read MAC failed: %s", esp_err_to_name(err));
    std::snprintf(ssid_, sizeof(ssid_), "%s-%02X%02X", Board::AP_SSID_PREFIX, mac[4], mac[5]);

    wifi_config_t cfg{};
    const std::size_t ssidLen = std::strlen(ssid_);
    std::memcpy(cfg.ap.ssid, ssid_, ssidLen);
    cfg.ap.ssid_len       = static_cast<uint8_t>(ssidLen);
    cfg.ap.channel        = kChannel;
    cfg.ap.max_connection = kMaxConnections;

    const char* pass = Board::AP_PASSWORD; // "" = open
    if (pass[0] == '\0') {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        std::strncpy(reinterpret_cast<char*>(cfg.ap.password), pass, sizeof(cfg.ap.password) - 1);
        cfg.ap.authmode         = WIFI_AUTH_WPA2_PSK;
        cfg.ap.pmf_cfg.required = false;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (err != ESP_OK) ESP_LOGE(TAG, "set_config failed: %s", esp_err_to_name(err));
    return err;
}

bool SoftApLink::ipInfo(esp_netif_ip_info_t& out) const
{
    if (state_ != LinkState::Up) return false;
    return esp_netif_get_ip_info(netif_, &out) == ESP_OK;
}

// ---------------------------------------------------------------- Events (sys_evt task)

void SoftApLink::onEvent(void* arg, esp_event_base_t, int32_t id, void* data)
{
    auto* self = static_cast<SoftApLink*>(arg);

    switch (id) {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "AP up");
        self->state_ = LinkState::Up;
        break;
    case WIFI_EVENT_AP_STOP:
        ESP_LOGD(TAG, "AP down");
        self->state_   = LinkState::Down;
        self->clients_ = 0;
        break;
    case WIFI_EVENT_AP_STACONNECTED: {
        auto* d = static_cast<wifi_event_ap_staconnected_t*>(data);
        const uint8_t n = self->clients_.fetch_add(1) + 1;
        ESP_LOGI(TAG, "client " MACSTR " joined (aid %u, %u clients)", MAC2STR(d->mac), d->aid, n);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        auto* d = static_cast<wifi_event_ap_stadisconnected_t*>(data);
        uint8_t c = self->clients_.load();
        while (c > 0 && !self->clients_.compare_exchange_weak(c, c - 1)) {}
        ESP_LOGI(TAG, "client " MACSTR " left (reason %u)", MAC2STR(d->mac), d->reason);
        break;
    }
    default: break;
    }
}
