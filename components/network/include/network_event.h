#ifndef NETWORK_EVENT_H_
#define NETWORK_EVENT_H_

#include "network_types.h"

enum class EvType : uint8_t {
    LinkUp, LinkDown,
    GotIp, LostIp,
    ConnectFailed,
    CfgSaved,
    Timeout
};
enum class FailReason : uint8_t { None, AuthFail, NoAp, Other };

struct Event {
    EvType     type;
    LinkId     link{};
    FailReason reason{};
    uint8_t    timer{};
    uint16_t   gen{};
};

#endif // NETWORK_EVENT_H_