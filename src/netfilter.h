
#pragma once

#include "utils.h"


class NetFilter {
private:
    uint32_t _ip;
    uint32_t _mask;
public:
    NetFilter(Slice &mask);
    bool match(uint32_t ip);
};
