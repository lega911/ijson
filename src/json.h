

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
    bool fail_on_disconnect;
    bool noid;
    
    int parse_object(ISlice buf);
    void reset();
};

class JData {
private:
    Slice _data;
    JsonParser main;
    JsonParser params;
    Slice _name;
    bool main_parsed;
    bool params_parsed;

    void ensure_main() {
        if(main_parsed) return;
        main_parsed = true;
        if(!_data.empty()) main.parse_object(_data);
    }
    void ensure_params() {
        if(params_parsed) return;
        params_parsed = true;
        ensure_main();
        if(main.params.empty()) return;
        if(main.params.ptr()[0] == '{') params.parse_object(main.params);
        else _name = main.params;
    }
public:
    JData() {
        reset();
    };
    void parse(ISlice data);
    void reset();

    Slice get_id();
    Slice get_method();
    Slice get_name();
    bool get_fail_on_disconnect();
    bool get_noid();
};

#endif /* JSON_H */
