#ifndef ILINK_HAL_H_
#define ILINK_HAL_H_

#include "network_types.h"

#include "esp_check.h"
#include "esp_netif.h"

class ILink_hal
{
private:
public:
    virtual ~ILink_hal() = default;
    virtual esp_err_t start() = 0;
    virtual void stop()  = 0;
    virtual LinkId id() const = 0;
    virtual LinkState state() const = 0;
    virtual esp_netif_t* netif() const = 0;
};

#endif // ILINK_HAL_H_