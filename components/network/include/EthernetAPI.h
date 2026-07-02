#ifndef ETHERNET_H_
#define ETHERNET_H_

#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

struct ethernetConfig {
    uint8_t phyAddr = 0;
    uint8_t powerPin = 5;
    uint8_t mdcPin = 23;
    uint8_t mdiPin = 18;
    emac_rmii_clock_gpio_t clkGpio = (emac_rmii_clock_gpio_t)17;
};

class EthernetAPI
{
private:
    static const char* TAG;
    ethernetConfig ethConfig;
    esp_eth_handle_t eth_handle;
    esp_netif_t* ethNetif;
    esp_eth_netif_glue_handle_t ethNetifGlue;
    bool ethIsInitialised;
    bool ethIsConnected;

    // -------------------------------------------------------------------------
    // Ethernet event handler + handlers
    // -------------------------------------------------------------------------

    /**
     * @brief ESP event handler for `ETH_EVENT`. Dispatches to dedicated handler methods.
     *
     * Handles: `ETHERNET_EVENT_CONNECTED`, `ETHERNET_EVENT_DISCONNECTED`,
     *          `ETHERNET_EVENT_START`, `ETHERNET_EVENT_STOP`
     */
    static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    /** @brief Logs the MAC address and "Link Up" on physical connection. */
    void onEthConnected();

    /** @brief Logs "Link Down" on physical disconnection. */
    void onEthDisconnected();

    /** @brief Logs that the Ethernet driver has started. */
    void onEthStart();

    /** @brief Logs that the Ethernet driver has stopped. */
    void onEthStop();

public:
    /**
     * @brief Default constructor. Does not start the Ethernet driver.
     */
    EthernetAPI();
    ~EthernetAPI();

    /**
     * @brief Initialise the Ethernet MAC/PHY, install the driver, attach the netif
     *        and start the interface. No-op if already initialised.
     */
    void initEthernet();

    /**
     * @brief Stop the Ethernet driver, detach the netif glue, uninstall the driver
     *        and destroy the netif.
     */
    void closeEthernet();

    /**
     * @brief Update the internal connected state.
     *
     * @note Called by the state machine in response to `IP_EVENT_ETH_GOT_IP` /
     *       `IP_EVENT_ETH_LOST_IP` — not from within this class.
     *
     * @param[in] connected True when a valid IP address has been obtained
     */
    void setEthIsConnected(bool connected);

    /** @return True if the Ethernet driver is currently initialised. */
    bool getEthIsInitialised();

    /**
     * @brief Gets current Ethernet IP Address.
     *
     * @param[out] out      Buffer receiving the null-terminated IP string; empty when not initialized.
     * @param[in]  out_size Size of `out` in bytes.
     */
    void getCurrentIp(char* out, size_t out_size);

    /** @return True if the Ethernet interface currently holds a valid IP address. */
    bool getEthIsConnected();
};

#endif /* ETHERNET_H_ */
