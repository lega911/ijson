

#ifndef JSON_H
#define JSON_H

#include "utils.h"

// --> {"jsonrpc": "2.0", "method": "subtract", "params": [42, 23], "id": 1}
// <-- {"jsonrpc": "2.0", "result": 19, "id": 1}

class JsonParser {
private:
    ISlice buf;
    int index = 0;
    Slice read_string();
    Slice read_object();
    void strip();

    inline char next() {
        if(index >= buf.size()) throw error::OutOfIndex();
        return buf.ptr()[index++];
    }
    inline char peek() {
        if(index >= buf.size()) throw error::OutOfIndex();
        return buf.ptr()[index];
    }
public:
    Slice method;
    Slice id;
    Slice params;
    Slice name;
    Slice fail_on_disconnect;
    
    int parse_object(ISlice buf);
};

#endif /* JSON_H */

