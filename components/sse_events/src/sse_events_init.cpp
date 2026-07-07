#include "sse_events_init.h"
#include "sse_events.hpp"

esp_err_t sse_events_init(httpd_handle_t server)
{
    return Sse_events::init(server);
}
