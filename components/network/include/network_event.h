#ifndef NETWORK_EVENT_H_
#define NETWORK_EVENT_H_

#include <cstdint>
#include <type_traits>
#include "ILink_hal.h"

/** @brief Normalized event types posted by links, timers and the NSM itself. */
enum class EvType : uint8_t {
    LinkUp,         ///< Cable plugged / WiFi associated (no IP yet)
    LinkDown,       ///< An established link went away
    GotIp,          ///< DHCP lease obtained
    LostIp,         ///< DHCP lease lost
    ConnectFailed,  ///< STA connection attempt failed, see Event::reason
    CfgSaved,       ///< New WiFi credentials were stored
    Timeout,        ///< NSM timer expired, see Event::timer / Event::gen
};

/** @brief Coarse cause of a failed STA connection. */
enum class FailReason : uint8_t { None, AuthFail, NoAp, Other };

/** @brief Event for the NSM queue. Must stay trivially copyable. */
struct Event {
    EvType     type;
    LinkId     link{};      ///< Source link (LinkUp/Down, GotIp/LostIp, ConnectFailed)
    FailReason reason{};    ///< Only for ConnectFailed / LinkDown from STA
    uint8_t    timer{};     ///< Only for Timeout: timer id
    uint16_t   gen{};       ///< Only for Timeout: timer generation at fire time
};

static_assert(std::is_trivially_copyable_v<Event>, "Event has to fit into a FreeRTOS queue");

constexpr const char* toString(EvType t)
{
    switch (t) {
    case EvType::LinkUp:        return "LinkUp";
    case EvType::LinkDown:      return "LinkDown";
    case EvType::GotIp:         return "GotIp";
    case EvType::LostIp:        return "LostIp";
    case EvType::ConnectFailed: return "ConnectFailed";
    case EvType::CfgSaved:      return "CfgSaved";
    case EvType::Timeout:       return "Timeout";
    }
    return "?";
}

constexpr const char* toString(FailReason r)
{
    switch (r) {
    case FailReason::None:     return "None";
    case FailReason::AuthFail: return "AuthFail";
    case FailReason::NoAp:     return "NoAp";
    case FailReason::Other:    return "Other";
    }
    return "?";
}

#endif // NETWORK_EVENT_H_
