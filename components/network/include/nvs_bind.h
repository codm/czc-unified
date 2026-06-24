#ifndef NETWORK_NVS_H_
#define NETWORK_NVS_H_

#include "esp_err.h"
#include "network_config.h"

constexpr const char* NVS_WIFI_NAMESPACE = "wifi_cfg";
constexpr const char* NVS_ETH_NAMESPACE  = "eth_cfg";

namespace NvsBinding {

    /**
     * @brief Check whether a WiFi configuration has previously been saved.
     *
     * @return true if a non-empty SSID is stored in NVS
     */
    bool wifiConfigExists();

    /**
     * @brief Read the full WiFi configuration from NVS.
     *
     *        Missing fields default to DHCP enabled and empty strings.
     *
     * @param[out] cfg  Destination struct
     *
     * @return `ESP_OK` on success, `ESP_ERR_NVS_NOT_FOUND` if namespace empty
     */
    esp_err_t readWifiConfig(wifi_config_data_t& cfg);

    /**
     * @brief Persist the full WiFi configuration to NVS.
     *
     * @param[in] cfg  Configuration to write
     *
     * @return `ESP_OK` on success
     */
    esp_err_t writeWifiConfig(const wifi_config_data_t& cfg);

    /**
     * @brief Read the Ethernet interface configuration from NVS.
     *
     *        Defaults to DHCP enabled when no entry exists.
     *
     * @param[out] cfg  Destination struct
     *
     * @return `ESP_OK` on success
     */
    esp_err_t readEthernetConfig(ethernet_config_data_t& cfg);

    /**
     * @brief Persist the Ethernet interface configuration to NVS.
     *
     * @param[in] cfg  Configuration to write
     *
     * @return `ESP_OK` on success
     */
    esp_err_t writeEthernetConfig(const ethernet_config_data_t& cfg);

} // namespace NvsBinding

#endif // NETWORK_NVS_H_
