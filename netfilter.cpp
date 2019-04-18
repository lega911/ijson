
#include "netfilter.h"
#include <arpa/inet.h>


NetFilter::NetFilter(Slice &mask) {
    std::string ip;
    int bits = 32;
    for(int i=0;i<mask.size();i++) {
        if(mask.ptr()[i] == '/') {
            ip.append(mask.ptr(), i);
            mask.remove(i + 1);
            bits = mask.atoi();
            break;
        }
    };
    if(ip.empty()) ip = mask.as_string();
    _ip = inet_addr(ip.c_str());

    _mask = 0xffffffff;
    bits = 32 - bits;
    for(int i=0;i<bits;i++) _mask = _mask << 1;
    // swap bytes
    char *b4 = (char*)&_mask;
    char t = b4[0]; b4[0] = b4[3]; b4[3] = t;
    t = b4[1]; b4[1] = b4[2]; b4[2] = t;

    _ip = _ip & _mask;
}

bool NetFilter::match(uint32_t ip) {
    return (ip & _mask) == _ip;
}