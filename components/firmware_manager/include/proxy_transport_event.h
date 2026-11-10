#include "esp_event.h"
#include "esp_log.h"

ESP_EVENT_DECLARE_BASE(CLIENT_CONNECTED_EVENT);

enum {
    CLIENT_CONNECTED,
    CLIENT_DISCONNECTED,
};