#include "sse_events_init.h"

#include "sse_events.hpp"

esp_err_t sse_events_init(httpd_handle_t server)
{
    static Sse_events sse_events(server);
    return sse_events.init();
}