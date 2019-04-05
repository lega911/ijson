
#include "buffer.h"


void Buffer::set(ISlice &s) {
    set(s.ptr(), s.size());
}

void Buffer::add(ISlice &s) {
    add(s.ptr(), s.size());
}
