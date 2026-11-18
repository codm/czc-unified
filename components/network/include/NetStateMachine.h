#ifndef NETSTATEMACHINE_H_
#define NETSTATEMACHINE_H_

#include <array>
#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_event.h"

#include "IEventSink.h"
#include "INetworkStatus.h"
#include "INetworkConfig.h"
#include "IConfigStore.h"
#include "ILink_hal.h"
#include "IWirelessLink.h"
#include "network_event.h"
#include "WifiCreds.h"

/** @brief NSM states. See network.md for the state diagram. */
enum class NetState : uint8_t {
    Init,
    EthConnect,
    OnlineEth,
    // ETH not available ---
    WifiConnect,
    WifiBackoff,
    OnlineWifi,
    SoftAp,
};

/** @brief Event base on the default event loop for NSM notifications. */
ESP_EVENT_DECLARE_BASE(NSM_EVENT);

/** @brief Event ids for NSM_EVENT. */
enum : int32_t {
    NSM_EVENT_STATE_CHANGED,    ///< Data: NsmStateEvent. Posted after every transition (entry actions done).
};

/** @brief Payload of NSM_EVENT_STATE_CHANGED. */
struct NsmStateEvent {
    NetState state;
    bool     online;
    LinkId   uplink;    ///< Only meaningful if online
};

/** @brief Tuning parameters of the NSM. */
struct NetParams {
    uint32_t ethLinkWaitMs      = 3000;   ///< EthConnect: wait for cable link
    uint32_t ethDhcpMs          = 10000;  ///< EthConnect: wait for DHCP once link is up
    uint32_t wifiConnectMs      = 15000;  ///< WifiConnect: max. duration of one attempt
    uint32_t backoffMinMs       = 2000;   ///< WifiBackoff: first delay
    uint32_t backoffMaxMs       = 60000;  ///< WifiBackoff: upper limit
    uint8_t  unverifiedAttempts = 3;      ///< Failures with unverified creds before SoftAp
    uint32_t taskStack          = 4096;
    UBaseType_t taskPrio        = 5;
};

/**
 * @brief Network policy: selects the uplink (ETH > STA > SoftAp).
 *
 * Runs in its own task and processes all events sequentially from a queue.
 * Status getters are safe from any task.
 */
class NetStateMachine final : public IEventSink,
                              public INetworkStatus,
                              public INetworkConfig {
public:
    /**
     * @param cfg    Persistent config store; must outlive the NSM.
     * @param params Timing/retry parameters.
     */
    explicit NetStateMachine(IConfigStore& cfg, NetParams params = {});
    ~NetStateMachine() override;

    NetStateMachine(const NetStateMachine&)            = delete;
    NetStateMachine& operator=(const NetStateMachine&) = delete;

    /**
     * @brief Wire the links. Must be called before start(). Links must outlive the NSM.
     * @param eth Ethernet link (passive).
     * @param sta WiFi station link.
     * @param ap  Soft AP link.
     */
    void      attach(ILink_hal& eth, IWirelessLink& sta, ILink_hal& ap);

    /**
     * @brief Create queue, timers and task. The task loads the config and starts ETH.
     * @return ESP_OK, ESP_ERR_INVALID_STATE (attach() missing / already started),
     *         ESP_ERR_NO_MEM, or an esp_timer error.
     */
    esp_err_t start();

    /** @brief Current state (for diagnostics, any task). */
    NetState state() const { return state_.load(); }

    /** @brief Number of events dropped due to a full queue. */
    uint32_t droppedEvents() const { return dropped_.load(); }

    /** @brief Printable state name. */
    static const char* name(NetState s);
    // IEventSink
    bool post(const Event& e) override;

    // INetworkStatus
    bool   isOnline() const override     { return online_.load(); }
    LinkId activeUplink() const override { return active_.load(); }
    bool   ipInfo(esp_netif_ip_info_t& out) const override;

    // INetworkConfig
    esp_err_t setWifiCredentials(const WifiCreds& creds) override;

private:
    enum class TimerId : uint8_t { Eth, Wifi, Backoff, Count };
    static constexpr std::size_t kTimerCount = static_cast<std::size_t>(TimerId::Count);
    static constexpr UBaseType_t kQueueLen   = 16;

    struct TimerSlot {
        esp_timer_handle_t     handle = nullptr;
        std::atomic<uint16_t>  gen{0};
        NetStateMachine*       sm = nullptr;
        TimerId                id{};
    };

    // Task
    static void taskEntry(void* arg);
    void run();
    void init();

    // State machine
    void dispatch(const Event& e);
    bool handleGlobal(const Event& e);
    void transition(NetState to);
    void enter(NetState s);
    void exit(NetState s);

    void onEthConnect(const Event& e);
    void onOnlineEth(const Event& e);
    void onWifiConnect(const Event& e);
    void onWifiBackoff(const Event& e);
    void onOnlineWifi(const Event& e);
    void onCfgSaved();
    void onWifiFailure();

    NetState fallbackTarget() const;
    bool inFallback() const;
    void setOnline(bool online, LinkId link = LinkId::Eth);
    static esp_err_t ensureStarted(ILink_hal& link);
    static bool isTimeout(const Event& e, TimerId t);
    static const char* timerName(uint8_t t);
    static void logIp(const char* what, const ILink_hal& link);

    // Timer
    void startTimer(TimerId t, uint32_t ms);
    void stopTimer(TimerId t);
    static void timerCallback(void* arg);
    TimerSlot& slot(TimerId t) { return timers_[static_cast<std::size_t>(t)]; }

    static constexpr const char* TAG = "nsm";

    IConfigStore& cfg_;
    const NetParams p_;

    ILink_hal*    eth_ = nullptr;
    IWirelessLink* sta_ = nullptr;
    ILink_hal*    ap_  = nullptr;

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t  task_  = nullptr;
    std::array<TimerSlot, kTimerCount> timers_{};

    WifiCreds creds_{};
    bool      verified_  = false;
    uint8_t   failCount_ = 0;
    uint32_t  backoffMs_ = 0;

    std::atomic<NetState> state_{NetState::Init};
    std::atomic<bool>     online_{false};
    std::atomic<LinkId>   active_{LinkId::Eth};
    std::atomic<uint32_t> dropped_{0};
};

#endif // NETSTATEMACHINE_H_
