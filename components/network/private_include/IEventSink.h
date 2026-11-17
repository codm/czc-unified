#ifndef IEVENTSINK_H_
#define IEVENTSINK_H_

#include "network_event.h"

class IEventSink
{
public:
    virtual ~IEventSink() = default;
    virtual bool post(const Event& e) = 0; // not allowed to block 
};

#endif // IEVENTSINK_H_
