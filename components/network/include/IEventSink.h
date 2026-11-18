#ifndef IEVENTSINK_H_
#define IEVENTSINK_H_

#include "network_event.h"

/** @brief Receiver of link/timer events (implemented by the NSM queue). */
class IEventSink
{
public:
    virtual ~IEventSink() = default;

    /**
     * @brief Enqueue an event. Never blocks, callable from any task
     *        (sys_evt, esp_timer, httpd). Not ISR-safe.
     * @return false if the event was dropped (queue full / not started).
     */
    virtual bool post(const Event& e) = 0;
};

#endif // IEVENTSINK_H_
