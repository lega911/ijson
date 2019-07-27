
#pragma once
#include "utils.h"


class Json {
public:
    Slice key;
    Slice value;

    Json() {reset();};
    Json(ISlice data) {load(data);};
    void load(ISlice data) {
        reset();
        _data = data;
    };
    void reset() {
        _status = 0;
        key.reset();
        value.reset();
        index = 0;
        _data.reset();
    };
    bool scan();
    void decode_value(Buffer &dest);
private:
    int _status;
    Slice _data;
    int index;
    void strip();
    inline char next(int shift=1) {
        if(index >= _data.size()) throw error::InvalidData();
        char a = _data.ptr()[index];
        index += shift;
        return a;
    }
    Slice read_string();
    Slice read_value();
    Slice read_object();
};
