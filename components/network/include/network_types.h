#ifndef NETWORK_TYPES_H_
#define NETWORK_TYPES_H_

#include <stdint.h>

enum class LinkId : uint8_t { Eth, Sta, Ap };
enum class LinkState : uint8_t { Stopped, Down, Connecting, Up };

struct ApInfo {
    char    ssid[33];
    int8_t  rssi;
    bool    secured;
};


#endif // NETWORK_TYPES_H_