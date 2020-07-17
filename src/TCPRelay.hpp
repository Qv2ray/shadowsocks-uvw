#pragma once
#include "shadowsocks.h"
#include <memory>

class TCPRelay
{
public:
    virtual ~TCPRelay() = default;
    virtual void stop() = 0;
    virtual int loopMain(profile_t&) = 0;
    static std::shared_ptr<TCPRelay> create();
};