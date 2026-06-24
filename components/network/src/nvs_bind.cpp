#include "nvs_bind.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "nvs-bind";

constexpr const char* KEY_SSID          = "ssid";
constexpr const char* KEY_PASSWORD      = "password";
constexpr const char* KEY_CONFIGURED    = "configured";
constexpr const char* KEY_DHCP          = "dhcp";
constexpr const char* KEY_STATIC_IP     = "static_ip";
constexpr const char* KEY_GATEWAY       = "gateway";
constexpr const char* KEY_DNS_PRIMARY   = "dns_pri";
constexpr const char* KEY_DNS_SECONDARY = "dns_sec";

namespace NvsBinding {

bool wifiConfigExists()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }
    uint8_t configured{0};
    nvs_get_u8(handle, KEY_CONFIGURED, &configured);
    nvs_close(handle);
    return configured == 1;
}

esp_err_t readWifiConfig(wifi_config_data_t& cfg)
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.dhcp = true;

    nvs_handle_t handle;
    esp_err_t ret{nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle)};
    if (ret != ESP_OK)
    {
        return ret;
    }

    size_t len;
    len = sizeof(cfg.ssid);          nvs_get_str(handle, KEY_SSID,          cfg.ssid,          &len);
    len = sizeof(cfg.password);      nvs_get_str(handle, KEY_PASSWORD,      cfg.password,      &len);
    len = sizeof(cfg.static_ip);     nvs_get_str(handle, KEY_STATIC_IP,     cfg.static_ip,     &len);
    len = sizeof(cfg.gateway);       nvs_get_str(handle, KEY_GATEWAY,       cfg.gateway,       &len);
    len = sizeof(cfg.dns_primary);   nvs_get_str(handle, KEY_DNS_PRIMARY,   cfg.dns_primary,   &len);
    len = sizeof(cfg.dns_secondary); nvs_get_str(handle, KEY_DNS_SECONDARY, cfg.dns_secondary, &len);

    uint8_t dhcp{1};
    nvs_get_u8(handle, KEY_DHCP, &dhcp);
    cfg.dhcp = static_cast<bool>(dhcp);

    nvs_close(handle);
    ESP_LOGI(TAG, "WiFi config read (ssid: %s)", cfg.ssid);
    return ESP_OK;
}

esp_err_t writeWifiConfig(const wifi_config_data_t& cfg)
{
    nvs_handle_t handle;
    esp_err_t ret{nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle)};
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_WIFI_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    nvs_set_str(handle, KEY_SSID,          cfg.ssid);
    nvs_set_str(handle, KEY_PASSWORD,      cfg.password);
    nvs_set_str(handle, KEY_STATIC_IP,     cfg.static_ip);
    nvs_set_str(handle, KEY_GATEWAY,       cfg.gateway);
    nvs_set_str(handle, KEY_DNS_PRIMARY,   cfg.dns_primary);
    nvs_set_str(handle, KEY_DNS_SECONDARY, cfg.dns_secondary);
    nvs_set_u8 (handle, KEY_DHCP,          static_cast<uint8_t>(cfg.dhcp));
    nvs_set_u8 (handle, KEY_CONFIGURED,    1);

    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "WiFi config written (ssid: %s)", cfg.ssid);
    return ret;
}

esp_err_t readEthernetConfig(ethernet_config_data_t& cfg)
{
    memset(&cfg, 0, sizeof(cfg));
    cfg.dhcp = true;

    nvs_handle_t handle;
    esp_err_t ret{nvs_open(NVS_ETH_NAMESPACE, NVS_READONLY, &handle)};
    if (ret != ESP_OK)
    {
        return ret;
    }

    size_t len;
    len = sizeof(cfg.static_ip);     nvs_get_str(handle, KEY_STATIC_IP,     cfg.static_ip,     &len);
    len = sizeof(cfg.gateway);       nvs_get_str(handle, KEY_GATEWAY,       cfg.gateway,       &len);
    len = sizeof(cfg.dns_primary);   nvs_get_str(handle, KEY_DNS_PRIMARY,   cfg.dns_primary,   &len);
    len = sizeof(cfg.dns_secondary); nvs_get_str(handle, KEY_DNS_SECONDARY, cfg.dns_secondary, &len);

    uint8_t dhcp{1};
    nvs_get_u8(handle, KEY_DHCP, &dhcp);
    cfg.dhcp = static_cast<bool>(dhcp);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t writeEthernetConfig(const ethernet_config_data_t& cfg)
{
    nvs_handle_t handle;
    esp_err_t ret{nvs_open(NVS_ETH_NAMESPACE, NVS_READWRITE, &handle)};
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_ETH_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    nvs_set_str(handle, KEY_STATIC_IP,     cfg.static_ip);
    nvs_set_str(handle, KEY_GATEWAY,       cfg.gateway);
    nvs_set_str(handle, KEY_DNS_PRIMARY,   cfg.dns_primary);
    nvs_set_str(handle, KEY_DNS_SECONDARY, cfg.dns_secondary);
    nvs_set_u8 (handle, KEY_DHCP,          static_cast<uint8_t>(cfg.dhcp));

    ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

} // namespace NvsBinding
