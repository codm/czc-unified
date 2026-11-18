#ifndef INETWORKSTATUS_H_
#define INETWORKSTATUS_H_

#include "esp_netif.h"
#include "ILink_hal.h"

/** @brief Read-only network status for services/web server. Any task. */
class INetworkStatus
{
public:
    virtual ~INetworkStatus() = default;

    /** @brief true if an uplink (ETH or STA) has an IP. */
    virtual bool   isOnline() const = 0;

    /** @brief Active uplink. Only meaningful while isOnline() is true. */
    virtual LinkId activeUplink() const = 0;

    /**
     * @brief IP configuration of the active uplink.
     * @return false if offline.
     */
    virtual bool   ipInfo(esp_netif_ip_info_t& out) const = 0;
};

#endif // INETWORKSTATUS_H_
