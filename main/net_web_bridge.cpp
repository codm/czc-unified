#include "net_web_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "NetStateMachine.h"
#include "IWifiScan.h"
#include "IConfigStore.h"
#include "ILink_hal.h"
#include "WifiCreds.h"
#include "sse_events.hpp"

#include "mdns.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* TAG = "net_bridge";

constexpr std::size_t kMaxScanResults = 20;
constexpr int kScanStartRetries       = 5;
constexpr int kScanPollMs             = 100;
constexpr int kScanTimeoutMs          = 10000;

// Legacy frontend modes (NET_STATE in restful.js)
enum LegacyMode : int { Init = 0, Ethernet = 1, Wifi = 2, AccessPoint = 3, RetryEthernet = 4, RetryWifi = 5 };

NetWebBridge::Context& ctxOf(void* p) { return *static_cast<NetWebBridge::Context*>(p); }

int legacyMode(NetState s)
{
    switch (s) {
    case NetState::Init:        return Init;
    case NetState::EthConnect:  return RetryEthernet;
    case NetState::OnlineEth:   return Ethernet;
    case NetState::WifiConnect:
    case NetState::WifiBackoff: return RetryWifi;
    case NetState::OnlineWifi:  return Wifi;
    case NetState::SoftAp:      return AccessPoint;
    }
    return Init;
}

// Uplink IP if online, AP IP in SoftAp, otherwise empty.
void currentIp(NetWebBridge::Context& c, NetState s, char* out, std::size_t len)
{
    out[0] = '\0';
    esp_netif_ip_info_t ip{};
    const bool ok = c.nsm.isOnline() ? c.nsm.ipInfo(ip)
                  : (s == NetState::SoftAp && c.ap.ipInfo(ip));
    if (ok) std::snprintf(out, len, IPSTR, IP2STR(&ip.ip));
}

esp_err_t getWifiConfig(void* ctx, wifi_config_data_t* out)
{
    std::memset(out, 0, sizeof(*out));
    out->dhcp = true;
    WifiCreds creds;
    if (ctxOf(ctx).cfg.loadCreds(creds)) {
        std::strncpy(out->ssid, creds.ssid, sizeof(out->ssid) - 1);
    }
    return ESP_OK;
}

esp_err_t setWifiConfig(void* ctx, const wifi_config_data_t* cfg)
{
    WifiCreds creds;
    if (!creds.assign(cfg->ssid, cfg->password)) {
        ESP_LOGW(TAG, "invalid WiFi creds (ssid len %u)", static_cast<unsigned>(std::strlen(cfg->ssid)));
        return ESP_ERR_INVALID_ARG;
    }
    if (!cfg->dhcp) ESP_LOGW(TAG, "static IP not supported, using DHCP");
    return ctxOf(ctx).nsm.setWifiCredentials(creds);
}

esp_err_t getEthConfig(void*, ethernet_config_data_t* out)
{
    std::memset(out, 0, sizeof(*out));
    out->dhcp = true;
    return ESP_OK;
}

esp_err_t setEthConfig(void*, const ethernet_config_data_t*)
{
    ESP_LOGW(TAG, "ethernet config not supported (DHCP only)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t getStatus(void* ctx, network_status_t* out)
{
    std::memset(out, 0, sizeof(*out));
    auto& c = ctxOf(ctx);
    const NetState s = c.nsm.state();
    out->mode = legacyMode(s);
    currentIp(c, s, out->ip, sizeof(out->ip));
    return ESP_OK;
}

esp_err_t setMdnsHostname(void*, const char* hostname)
{
    return mdns_hostname_set(hostname);
}

// Blocking scan (old API contract): runs in the httpd task.
esp_err_t scanWifi(void* ctx, scan_shortend_record_t** out, uint16_t* count)
{
    *out   = nullptr;
    *count = 0;
    IWifiScan& scan = ctxOf(ctx).scan;

    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < kScanStartRetries; ++i) {
        err = scan.startScan();
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) break;
        vTaskDelay(pdMS_TO_TICKS(500));             // driver busy (connecting)
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan not possible: %s", esp_err_to_name(err));
        return err;
    }

    int waited = 0;
    while (scan.scanState() == ScanState::Running && waited < kScanTimeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(kScanPollMs));
        waited += kScanPollMs;
    }
    if (scan.scanState() != ScanState::Done) {
        ESP_LOGW(TAG, "scan failed or timed out");
        return ESP_FAIL;
    }

    ApInfo aps[kMaxScanResults];
    const std::size_t n = scan.results(aps, kMaxScanResults);
    if (n == 0) return ESP_OK;

    auto* recs = static_cast<scan_shortend_record_t*>(std::calloc(n, sizeof(scan_shortend_record_t)));
    if (!recs) return ESP_ERR_NO_MEM;
    for (std::size_t i = 0; i < n; ++i) {
        std::memcpy(recs[i].ssid, aps[i].ssid, sizeof(recs[i].ssid));
        recs[i].rssi            = aps[i].rssi;
        recs[i].authmode        = aps[i].secured ? 1 : 0;   // frontend only checks != 0
        recs[i].primary_channel = 0;                          // not provided by IWifiScan
    }
    *out   = recs;                                            // freed by the web server
    *count = static_cast<uint16_t>(n);
    return ESP_OK;
}

void onStateChanged(void* arg, esp_event_base_t, int32_t, void* data)
{
    auto& c  = ctxOf(arg);
    auto* ev = static_cast<const NsmStateEvent*>(data);
    char ip[16];
    currentIp(c, ev->state, ip, sizeof(ip));
    Sse_events::network::post_network_state_change(legacyMode(ev->state), ip);
}

} // namespace

namespace NetWebBridge {

void fillCallbacks(web_network_callbacks_t* cbs, Context* ctx)
{
    cbs->get_wifi_config       = getWifiConfig;
    cbs->set_wifi_config       = setWifiConfig;
    cbs->get_ethernet_config   = getEthConfig;
    cbs->set_ethernet_config   = setEthConfig;
    cbs->get_network_status    = getStatus;
    cbs->set_mdns_hostname     = setMdnsHostname;
    cbs->get_wifi_scan_results = scanWifi;
    cbs->ctx                   = ctx;
}

esp_err_t forwardStateToSse(Context* ctx)
{
    return esp_event_handler_register(NSM_EVENT, NSM_EVENT_STATE_CHANGED, &onStateChanged, ctx);
}

} // namespace NetWebBridge
