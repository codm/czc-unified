#ifndef INETLINK_H_
#define INETLINK_H_

#include "ILink_hal.h"
#include "esp_check.h"
#include "esp_netif_types.h"

class INetLink : ILink_hal
{
private:
public:
    virtual esp_err_t connect() = 0; 
    virtual void disconnect() = 0;
    virtual bool ipInfo(esp_netif_ip_info_t& out) const = 0;
    virtual void setDefaultRoute(bool primary) = 0;
};

#endif // INETLINK_H_