#ifndef IWIFISCAN_H_
#define IWIFISCAN_H_

#include <span>
#include "network_types.h"
#include "esp_check.h"

class IWifiScan
{
private:
public:
    virtual ~IWifiScan() = default;
    virtual esp_err_t startScan() = 0;
    virtual bool results(std::span<ApInfo>& out) const = 0;
};

#endif // IWIFISCAN_H_