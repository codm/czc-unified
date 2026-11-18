#ifndef NET_WEB_BRIDGE_H_
#define NET_WEB_BRIDGE_H_

#include "esp_err.h"
#include "network_config.h"

class NetStateMachine;
class IWifiScan;
class IConfigStore;
class ILink_hal;

// Temporary glue between the C++ network component and the C web server.
// Goes away once the web server is ported to C++.
namespace NetWebBridge {

struct Context {
    NetStateMachine& nsm;
    IWifiScan&       scan;
    IConfigStore&    cfg;
    ILink_hal&       ap;
};

/** @brief Fill the C callback table for esp_br_web_start(). ctx must outlive the web server. */
void fillCallbacks(web_network_callbacks_t* cbs, Context* ctx);

/** @brief Forward NSM_EVENT_STATE_CHANGED to the SSE stream. ctx must stay valid. */
esp_err_t forwardStateToSse(Context* ctx);

} // namespace NetWebBridge

#endif // NET_WEB_BRIDGE_H_
