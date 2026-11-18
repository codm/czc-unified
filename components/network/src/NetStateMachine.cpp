
#include "NetStateMachine.h"

#include <algorithm>
#include "esp_log.h"
#include "esp_random.h"

ESP_EVENT_DEFINE_BASE(NSM_EVENT);

// ================================================================ Setup

NetStateMachine::NetStateMachine(IConfigStore& cfg, NetParams params)
    : cfg_(cfg), p_(params), backoffMs_(params.backoffMinMs)
{
}

NetStateMachine::~NetStateMachine()
{
    if (task_)  vTaskDelete(task_);
    for (auto& t : timers_) {
        if (t.handle) { esp_timer_stop(t.handle); esp_timer_delete(t.handle); }
    }
    if (queue_) vQueueDelete(queue_);
}

void NetStateMachine::attach(ILink_hal& eth, IWirelessLink& sta, ILink_hal& ap)
{
    eth_ = &eth;
    sta_ = &sta;
    ap_  = &ap;
}

esp_err_t NetStateMachine::start()
{
    if (!eth_ || !sta_ || !ap_) {
        ESP_LOGE(TAG, "start: attach() not called");
        return ESP_ERR_INVALID_STATE;
    }
    if (task_) {
        ESP_LOGW(TAG, "start: already running");
        return ESP_ERR_INVALID_STATE;
    }

    queue_ = xQueueCreate(kQueueLen, sizeof(Event));
    if (!queue_) {
        ESP_LOGE(TAG, "start: queue alloc failed");
        return ESP_ERR_NO_MEM;
    }

    static constexpr const char* kTimerNames[kTimerCount] = {"nsm_eth", "nsm_wifi", "nsm_backoff"};
    for (std::size_t i = 0; i < kTimerCount; ++i) {
        timers_[i].sm = this;
        timers_[i].id = static_cast<TimerId>(i);
        esp_timer_create_args_t args{};
        args.callback = &NetStateMachine::timerCallback;
        args.arg      = &timers_[i];
        args.name     = kTimerNames[i];
        esp_err_t err = esp_timer_create(&args, &timers_[i].handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "start: timer '%s' create failed: %s", kTimerNames[i], esp_err_to_name(err));
            return err;
        }
    }

    // Everything else (load NVS, start ETH) happens in the task,
    // so state is only touched there.
    if (xTaskCreate(&NetStateMachine::taskEntry, "nsm", p_.taskStack, this, p_.taskPrio, &task_) != pdPASS) {
        ESP_LOGE(TAG, "start: task create failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}

// ================================================================ Public interfaces

bool NetStateMachine::post(const Event& e)
{
    if (!queue_) {
        ESP_LOGW(TAG, "post %s before start(), dropped", toString(e.type));
        return false;
    }
    if (xQueueSend(queue_, &e, 0) != pdTRUE) {     // never block
        const uint32_t n = dropped_.fetch_add(1) + 1;
        ESP_LOGE(TAG, "queue full, %s/%s dropped (total %lu)",
                 toString(e.type), toString(e.link), static_cast<unsigned long>(n));
        return false;
    }
    return true;
}

bool NetStateMachine::ipInfo(esp_netif_ip_info_t& out) const
{
    if (!online_) return false;
    const ILink_hal* link = (active_ == LinkId::Eth) ? eth_ : static_cast<const ILink_hal*>(sta_);
    return link && link->ipInfo(out);
}

esp_err_t NetStateMachine::setWifiCredentials(const WifiCreds& creds)
{
    // Runs in the httpd task: only store and notify, never touch NSM state.
    if (!creds.valid()) {
        ESP_LOGW(TAG, "setWifiCredentials: invalid creds");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = cfg_.saveCreds(creds);          // resets verified
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setWifiCredentials: save failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "new creds for '%s' stored", creds.ssid);
    return post({.type = EvType::CfgSaved}) ? ESP_OK : ESP_ERR_NO_MEM;
}

// ================================================================ Task

void NetStateMachine::taskEntry(void* arg)
{
    static_cast<NetStateMachine*>(arg)->run();
}

void NetStateMachine::run()
{
    init();
    Event e;
    for (;;) {
        if (xQueueReceive(queue_, &e, portMAX_DELAY) == pdTRUE) {
            dispatch(e);
        }
    }
}

void NetStateMachine::init()
{
    if (cfg_.loadCreds(creds_)) {
        sta_->setCredentials(creds_);
    } else {
        creds_ = {};
    }
    verified_ = cfg_.isVerified();
    ESP_LOGI(TAG, "init: creds=%d ssid='%s' verified=%d", creds_.valid(), creds_.ssid, verified_);

    // ETH always runs, regardless of state, so a plugged cable can take over anytime.
    esp_err_t err = eth_->start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ETH start failed (%s), continuing without ETH", esp_err_to_name(err));
        transition(fallbackTarget());
        return;
    }
    transition(NetState::EthConnect);
}

// ================================================================ Dispatch

void NetStateMachine::dispatch(const Event& e)
{
    // Drop stale timeouts: timer was restarted or stopped since.
    if (e.type == EvType::Timeout) {
        if (e.timer >= kTimerCount || e.gen != timers_[e.timer].gen.load()) {
            ESP_LOGD(TAG, "[%s] stale timeout %s (gen %u) ignored",
                     name(state_.load()), timerName(e.timer), e.gen);
            return;
        }
        ESP_LOGD(TAG, "[%s] timeout %s", name(state_.load()), timerName(e.timer));
    } else {
        ESP_LOGD(TAG, "[%s] event %s link=%s reason=%s", name(state_.load()),
                 toString(e.type), toString(e.link), toString(e.reason));
    }

    if (handleGlobal(e)) return;

    switch (state_.load()) {
    case NetState::EthConnect:  onEthConnect(e);  break;
    case NetState::OnlineEth:   onOnlineEth(e);   break;
    case NetState::WifiConnect: onWifiConnect(e); break;
    case NetState::WifiBackoff: onWifiBackoff(e); break;
    case NetState::OnlineWifi:  onOnlineWifi(e);  break;
    case NetState::SoftAp:      break;            // waits for CfgSaved / ETH (global)
    case NetState::Init:        break;
    }
}

// Events with the same effect in every state
bool NetStateMachine::handleGlobal(const Event& e)
{
    // ETH has priority: IP via cable -> OnlineEth from any state.
    if (e.type == EvType::GotIp && e.link == LinkId::Eth) {
        if (state_ != NetState::OnlineEth) {
            ESP_LOGI(TAG, "ETH got IP, taking over as uplink");
            transition(NetState::OnlineEth);
        }
        return true;
    }
    if (e.type == EvType::CfgSaved) {
        onCfgSaved();
        return true;
    }
    return false;
}

// ================================================================ States

void NetStateMachine::onEthConnect(const Event& e)
{
    if (e.link == LinkId::Eth && e.type == EvType::LinkUp) {
        ESP_LOGI(TAG, "ETH link up, waiting %lu ms for DHCP", static_cast<unsigned long>(p_.ethDhcpMs));
        startTimer(TimerId::Eth, p_.ethDhcpMs);      // cable present: now wait for DHCP
    } else if (e.link == LinkId::Eth && e.type == EvType::LinkDown) {
        ESP_LOGI(TAG, "ETH link down while connecting");
        transition(fallbackTarget());
    } else if (isTimeout(e, TimerId::Eth)) {
        ESP_LOGI(TAG, "ETH: no %s in time", eth_->state() == LinkState::Connecting ? "DHCP lease" : "link");
        transition(fallbackTarget());
    }
}

void NetStateMachine::onOnlineEth(const Event& e)
{
    if (e.link != LinkId::Eth) return;
    if (e.type == EvType::LinkDown) {
        ESP_LOGW(TAG, "ETH cable unplugged");
        transition(fallbackTarget());
    } else if (e.type == EvType::LostIp) {
        ESP_LOGW(TAG, "ETH lost IP, waiting for DHCP");
        transition(NetState::EthConnect);             // cable still plugged, DHCP again
    }
}

void NetStateMachine::onWifiConnect(const Event& e)
{
    if (e.link == LinkId::Sta) {
        switch (e.type) {
        case EvType::GotIp:
            transition(NetState::OnlineWifi);
            return;
        case EvType::ConnectFailed:
            ESP_LOGW(TAG, "WiFi connect to '%s' failed: %s", creds_.ssid, toString(e.reason));
            onWifiFailure();
            return;
        case EvType::LinkDown:
            ESP_LOGW(TAG, "WiFi dropped before IP: %s", toString(e.reason));
            onWifiFailure();
            return;
        case EvType::LinkUp:
            ESP_LOGI(TAG, "WiFi associated, waiting for DHCP");
            return;
        default:
            return;
        }
    }
    if (isTimeout(e, TimerId::Wifi)) {
        ESP_LOGW(TAG, "WiFi connect timeout after %lu ms (STA %s)",
                 static_cast<unsigned long>(p_.wifiConnectMs), toString(sta_->state()));
        sta_->disconnect();                           // abort attempt, posts no event
        onWifiFailure();
    }
}

void NetStateMachine::onWifiBackoff(const Event& e)
{
    if (isTimeout(e, TimerId::Backoff)) transition(NetState::WifiConnect);
}

void NetStateMachine::onOnlineWifi(const Event& e)
{
    if (e.link != LinkId::Sta) return;
    if (e.type == EvType::LinkDown || e.type == EvType::LostIp) {
        ESP_LOGW(TAG, "WiFi uplink lost (%s, reason %s), reconnecting",
                 toString(e.type), toString(e.reason));
        transition(NetState::WifiConnect);            // was online -> retry immediately
    }
}

void NetStateMachine::onWifiFailure()
{
    ++failCount_;
    ESP_LOGW(TAG, "WiFi attempt %u failed (verified=%d)", failCount_, verified_);

    // Creds never worked and failed repeatedly -> probably a typo
    if (!verified_ && failCount_ >= p_.unverifiedAttempts) {
        ESP_LOGW(TAG, "unverified creds failed %u times, opening portal", failCount_);
        transition(NetState::SoftAp);
    } else {
        transition(NetState::WifiBackoff);
    }
}

void NetStateMachine::onCfgSaved()
{
    if (!cfg_.loadCreds(creds_)) {
        ESP_LOGE(TAG, "CfgSaved, but no valid creds in store");
        return;
    }
    verified_  = cfg_.isVerified();                   // false after saveCreds()
    failCount_ = 0;
    backoffMs_ = p_.backoffMinMs;
    sta_->setCredentials(creds_);

    // Configured via ETH: creds are kept and used once the cable is unplugged.
    if (inFallback()) {
        ESP_LOGI(TAG, "new creds for '%s', reconnecting", creds_.ssid);
        transition(NetState::WifiConnect);
    } else {
        ESP_LOGI(TAG, "new creds for '%s' stored, used when ETH goes down", creds_.ssid);
    }
}

// ================================================================ Entry / Exit

void NetStateMachine::transition(NetState to)
{
    const NetState from = state_.load();
    ESP_LOGI(TAG, "%s -> %s", name(from), name(to));
    exit(from);
    state_ = to;
    enter(to);

    // Notify consumers (web server, status LED, ...) via the default event loop.
    const NsmStateEvent ev{.state = to, .online = online_.load(), .uplink = active_.load()};
    esp_err_t err = esp_event_post(NSM_EVENT, NSM_EVENT_STATE_CHANGED, &ev, sizeof(ev), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "state event for %s not posted: %s", name(to), esp_err_to_name(err));
    }
}

void NetStateMachine::enter(NetState s)
{
    switch (s) {
    case NetState::EthConnect: {
        setOnline(false);
        // Coming from OnlineEth (IP lost): cable already plugged.
        const bool linkUp = eth_->state() == LinkState::Connecting;
        const uint32_t ms = linkUp ? p_.ethDhcpMs : p_.ethLinkWaitMs;
        ESP_LOGD(TAG, "waiting %lu ms for ETH %s", static_cast<unsigned long>(ms), linkUp ? "DHCP" : "link");
        startTimer(TimerId::Eth, ms);
        break;
    }

    case NetState::OnlineEth:
        setOnline(true, LinkId::Eth);
        logIp("online via ETH", *eth_);
        sta_->stop();                                 // WiFi fully off, radio free
        ap_->stop();
        break;

    case NetState::WifiConnect: {
        setOnline(false);
        ESP_LOGI(TAG, "connecting to '%s'", creds_.ssid);
        esp_err_t err = ensureStarted(*sta_);
        if (err == ESP_OK) {
            err = sta_->connect();
            if (err != ESP_OK) ESP_LOGE(TAG, "STA connect() failed: %s", esp_err_to_name(err));
        }
        if (err != ESP_OK) {
            // No transition inside enter(): queue the failure as event.
            post({.type = EvType::ConnectFailed, .link = LinkId::Sta, .reason = FailReason::Other});
            break;
        }
        startTimer(TimerId::Wifi, p_.wifiConnectMs);
        break;
    }

    case NetState::WifiBackoff: {
        const uint32_t jitter = esp_random() % (backoffMs_ / 4 + 1);
        ESP_LOGI(TAG, "retry WiFi in %lu ms", static_cast<unsigned long>(backoffMs_ + jitter));
        startTimer(TimerId::Backoff, backoffMs_ + jitter);
        backoffMs_ = std::min(backoffMs_ * 2, p_.backoffMaxMs);
        break;
    }

    case NetState::OnlineWifi:
        setOnline(true, LinkId::Sta);
        logIp("online via WiFi", *sta_);
        if (!verified_) {
            verified_ = true;
            esp_err_t err = cfg_.setVerified(true);   // from now on never auto-fallback to AP
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "persisting verified flag failed: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "creds for '%s' verified", creds_.ssid);
            }
        }
        failCount_ = 0;
        backoffMs_ = p_.backoffMinMs;
        ap_->stop();                                  // provisioning done
        break;

    case NetState::SoftAp:
        setOnline(false);
        failCount_ = 0;
        ensureStarted(*ap_);                          // AP first ...
        ensureStarted(*sta_);                         // ... then STA -> APSTA, needed for scan
        sta_->disconnect();
        ESP_LOGI(TAG, "provisioning portal active (creds=%d)", creds_.valid());
        break;

    case NetState::Init:
        break;
    }
}

void NetStateMachine::exit(NetState s)
{
    switch (s) {
    case NetState::EthConnect:  stopTimer(TimerId::Eth);     break;
    case NetState::WifiConnect: stopTimer(TimerId::Wifi);    break;
    case NetState::WifiBackoff: stopTimer(TimerId::Backoff); break;
    default: break;
    }
    // The AP intentionally stays on when leaving SoftAp: the portal remains
    // reachable during the first attempt with new creds. It is only stopped
    // on success (OnlineWifi / OnlineEth).
}

// ================================================================ Helpers

NetState NetStateMachine::fallbackTarget() const
{
    return creds_.valid() ? NetState::WifiConnect : NetState::SoftAp;
}

bool NetStateMachine::inFallback() const
{
    switch (state_.load()) {
    case NetState::WifiConnect:
    case NetState::WifiBackoff:
    case NetState::OnlineWifi:
    case NetState::SoftAp:
        return true;
    default:
        return false;
    }
}

void NetStateMachine::setOnline(bool online, LinkId link)
{
    active_ = link;                                   // link first, then flag
    online_ = online;
}

esp_err_t NetStateMachine::ensureStarted(ILink_hal& link)
{
    if (link.state() != LinkState::Stopped) return ESP_OK;
    ESP_LOGD(TAG, "starting %s link", toString(link.id()));
    esp_err_t err = link.start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s link start failed: %s", toString(link.id()), esp_err_to_name(err));
    }
    return err;
}

bool NetStateMachine::isTimeout(const Event& e, TimerId t)
{
    return e.type == EvType::Timeout && e.timer == static_cast<uint8_t>(t);
}

void NetStateMachine::logIp(const char* what, const ILink_hal& link)
{
    esp_netif_ip_info_t ip{};
    if (link.ipInfo(ip)) {
        ESP_LOGI(TAG, "%s: ip " IPSTR " gw " IPSTR, what, IP2STR(&ip.ip), IP2STR(&ip.gw));
    } else {
        ESP_LOGI(TAG, "%s", what);
    }
}

const char* NetStateMachine::name(NetState s)
{
    switch (s) {
    case NetState::Init:        return "INIT";
    case NetState::EthConnect:  return "ETH_CONNECT";
    case NetState::OnlineEth:   return "ONLINE_ETH";
    case NetState::WifiConnect: return "WIFI_CONNECT";
    case NetState::WifiBackoff: return "WIFI_BACKOFF";
    case NetState::OnlineWifi:  return "ONLINE_WIFI";
    case NetState::SoftAp:      return "SOFT_AP";
    }
    return "?";
}

const char* NetStateMachine::timerName(uint8_t t)
{
    switch (static_cast<TimerId>(t)) {
    case TimerId::Eth:     return "eth";
    case TimerId::Wifi:    return "wifi";
    case TimerId::Backoff: return "backoff";
    default:               return "?";
    }
}

// ================================================================ Timer

void NetStateMachine::startTimer(TimerId t, uint32_t ms)
{
    auto& s = slot(t);
    esp_timer_stop(s.handle);                         // "not running" error is fine
    s.gen.fetch_add(1);                               // invalidate queued timeouts
    esp_err_t err = esp_timer_start_once(s.handle, static_cast<uint64_t>(ms) * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer %s start failed: %s", timerName(static_cast<uint8_t>(t)), esp_err_to_name(err));
    }
}

void NetStateMachine::stopTimer(TimerId t)
{
    auto& s = slot(t);
    esp_timer_stop(s.handle);
    s.gen.fetch_add(1);                               // also on stop: drop queued timeouts
}

void NetStateMachine::timerCallback(void* arg)        // esp_timer task
{
    auto* s = static_cast<TimerSlot*>(arg);
    s->sm->post({.type  = EvType::Timeout,
                 .timer = static_cast<uint8_t>(s->id),
                 .gen   = s->gen.load()});
}
