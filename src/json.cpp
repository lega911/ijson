
#include "json.h"


void json::unescape(Buffer &s, int start) {
    char *p = s.ptr();
    int n = start;
    bool prefix = false;
    for(int i=start;i<s.size();i++) {
        if(!prefix && p[i] == '\\') {
            prefix = true;
            continue;
        }
        prefix = false;
        p[n] = p[i];
        n++;
    }
    s.resize(0, n);
}


bool Json::scan() {
    if(_data.empty()) return false;
    strip();
    if(_status == 0) {
        if(next() != '{') throw error::InvalidData();
        strip();
        if(next() == '}') return false;
        index--;
    } else if(_status == 1) {
        char a = next();
        if(a == '}') return false;
        if(a != ',') throw error::InvalidData();
    }

    strip();
    key = read_string();
    strip();
    if(next() != ':') throw error::InvalidData();
    strip();
    char a = next();
    index--;

    if(a == '"') value = read_string();
    else if(a == '{' || a == '[') value = read_object();
    else value = read_value();

    _status = 1;
    return true;
}


void Json::strip() {
    while(index < _data.size()) {
        char a = _data.ptr()[index];
        if(a == ' ' || a == '\n' || a == '\r' || a == '\t') {
            index++;
            continue;
        }
        return;
    }
    throw error::InvalidData();
}


Slice Json::read_string() {
    if(next() != '"') throw error::InvalidData();
    int start = index;
    const char *ptr = _data.ptr();
    for(;index < _data.size();index++) {
        if(ptr[index] == '"' && ptr[index-1] != '\\') {
            index++;
            return Slice(&ptr[start], index - start - 1);
        }
    }
    throw error::InvalidData();
}


Slice Json::read_value() {
    // 0-9, null, true, false
    int start = index;
    char a;
    bool is_number = true;
    while(true) {
        a = next();
        if(a == ' ' || a == ',' || a == '}' || a == '\n' || a == '\r') break;
        if(a < '0' || a > '9') is_number = false;
        if(!is_number && (index - start > 5)) throw error::InvalidData();
    }
    index--;
    Slice result(&_data.ptr()[start], index - start);
    if(!is_number) {
        if(!(result == "null" || result == "true" || result == "false")) throw error::InvalidData();
    }
    return result;
}


Slice Json::read_object() {
    int lvl = 0;
    int start = index;
    while(true) {
        char a = next();
        if(a == '{' || a == '[') {
            lvl++;
        } else if(a == '}' || a == ']') {
            lvl--;
            if(lvl==0) return Slice(&_data.ptr()[start], index - start);
        } else if(a == '"') {
            index--;
            read_string();
            continue;
        }
    }
}


void Json::decode_value(Buffer &dest) {
    dest.set(value);
    json::unescape(dest);
}
