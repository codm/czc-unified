#ifndef ILINK_HAL_H_
#define ILINK_HAL_H_

#include <cstdint>

#include "esp_check.h"
#include "esp_netif.h"

/** @brief Identifies a network link. */
enum class LinkId : uint8_t { Eth, Sta, Ap };

/** @brief Mechanical state of a link. */
enum class LinkState : uint8_t {
    Stopped,    ///< Driver/netif not created
    Down,       ///< Started, no link
    Connecting, ///< Link up / associating, no IP yet (AP: starting)
    Up,         ///< IP available (AP: running)
};

constexpr const char* toString(LinkId id)
{
    switch (id) {
    case LinkId::Eth: return "ETH";
    case LinkId::Sta: return "STA";
    case LinkId::Ap:  return "AP";
    }
    return "?";
}

constexpr const char* toString(LinkState s)
{
    switch (s) {
    case LinkState::Stopped:    return "Stopped";
    case LinkState::Down:       return "Down";
    case LinkState::Connecting: return "Connecting";
    case LinkState::Up:         return "Up";
    }
    return "?";
}

/**
 * @brief Common link interface used by the NSM.
 *
 * A link wraps one ESP-IDF driver + netif, translates raw ESP-IDF events
 * into Event and posts them to an IEventSink. It makes no policy decisions.
 */
class ILink_hal
{
public:
    virtual ~ILink_hal() = default;

    /**
     * @brief Create driver/netif and start the link. Does not connect (STA).
     * @return ESP_OK, ESP_ERR_INVALID_STATE if already started, or a driver error.
     */
    virtual esp_err_t start() = 0;

    /** @brief Stop and free all resources. No-op if stopped. Posts no event. */
    virtual void stop()  = 0;

    /** @brief Fixed link id. */
    virtual LinkId id() const = 0;

    /** @brief Current link state (atomic, any task). */
    virtual LinkState state() const = 0;

    /** @brief Underlying netif, nullptr if stopped. */
    virtual esp_netif_t* netif() const = 0;

    /**
     * @brief Current IP configuration.
     * @return false if the link has no IP (state() != Up).
     */
    virtual bool ipInfo(esp_netif_ip_info_t& out) const = 0;
};

#endif // ILINK_HAL_H_
