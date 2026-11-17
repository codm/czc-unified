#ifndef CZC_NETWORK_CONFIG_H_
#define CZC_NETWORK_CONFIG_H_

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Full WiFi interface configuration (credentials + IP settings).
 */
typedef struct {
    char ssid[64];           /**< Access point SSID */
    char password[64];       /**< WPA2 password — empty string means open network */
    bool dhcp;               /**< true = DHCP, false = static IP */
    char static_ip[16];      /**< Static IPv4 address (ignored when dhcp = true) */
    char gateway[16];        /**< Gateway IPv4 address */
    char dns_primary[16];    /**< Primary DNS server */
    char dns_secondary[16];  /**< Secondary DNS server */
} wifi_config_data_t;

/**
 * @brief Full Ethernet interface configuration (IP settings).
 */
typedef struct {
    bool dhcp;               /**< true = DHCP, false = static IP */
    char static_ip[16];      /**< Static IPv4 address (ignored when dhcp = true) */
    char gateway[16];        /**< Gateway IPv4 address */
    char dns_primary[16];    /**< Primary DNS server */
    char dns_secondary[16];  /**< Secondary DNS server */
} ethernet_config_data_t;

/**
 * @brief Current network connection status.
 */
typedef struct {
    int  mode;       /**< Active NetworkState cast to int */
    char ip[16];     /**< Current IPv4 address — empty string if not connected */
} network_status_t;

/**
 * @brief shortend version of wifi_ap_record_t with data for frontend display
 */
typedef struct 
{
    uint8_t ssid[33];
    int8_t rssi;        // Signal strength of AP - dbm between -100 to 10
    uint8_t authmode;   // wifi_auth_mode_t as plain uint8_t
    uint8_t primary_channel; 
}scan_shortend_record_t;

/**
 * @brief Network config callbacks — filled by NetworkStateMachine.
 *        ctx is a NetworkStateMachine* cast to void*.
 */
typedef struct {
    esp_err_t (*get_wifi_config)    (void *ctx, wifi_config_data_t *out);
    esp_err_t (*set_wifi_config)    (void *ctx, const wifi_config_data_t *cfg);
    esp_err_t (*get_ethernet_config)(void *ctx, ethernet_config_data_t *out);
    esp_err_t (*set_ethernet_config)(void *ctx, const ethernet_config_data_t *cfg);
    esp_err_t (*get_network_status) (void *ctx, network_status_t *out);
    esp_err_t (*set_mdns_hostname) (void *ctx, const char* hostname);
    esp_err_t (*get_wifi_scan_results)(void *ctx, scan_shortend_record_t **out, uint16_t *count);
    void *ctx;
} web_network_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif // CZC_NETWORK_CONFIG_H_
