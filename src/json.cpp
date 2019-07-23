
#include "json.h"

void JsonParser::reset() {
    this->method.clear();
    this->id.clear();
    this->params.clear();
    this->name.clear();
    this->info.clear();
    this->fail_on_disconnect = false;
    this->noid = false;
};

void unescape(Buffer &s) {
    char *p = s.ptr();
    int n = 0;
    for(int i=0;i<s.size();i++) {
        if(p[i] == '\\') continue;
        p[n] = p[i];
        n++;
    }
    s.resize(0, n);
}

int JsonParser::_parse_object(ISlice buf, bool is_params) {
    if(buf.size() < 10) throw error::InvalidData();
    const char *ptr = buf.ptr();
    if(ptr[0] != '{') throw error::InvalidData();
    this->buf = buf;

    index = 1;
    bool skip_body = false;

    Slice key, value;
    reset();

    for(;index < buf.size();) {
        strip();
        key = read_string();
        strip();
        if(next() != ':') throw error::InvalidData();
        strip();
        char a = peek();
        if(a == '"') value = read_string();
        else if(a == '{' || a == '[') {
            // inner object
            int lvl = 0;
            int start = index;
            for(;index<buf.size();) {
                a = ptr[index];
                if(a == '{' || a == '[') {
                    lvl++;
                } else if(a == '}' || a == ']') {
                    lvl--;
                    if(lvl==0) {
                        index++;
                        break;
                    }
                } else if(a == '"') {
                    read_string();
                    continue;
                }
                index++;
            }
            value = Slice(&ptr[start], index - start);
        } else value = read_object();

        if(key.equal("method")) {
            this->method = value;
            if(!is_params && !value.starts_with("/rpc/") && !value.starts_with("rpc/")) {
                skip_body = true;
                if(this->id.valid()) return 1;
            }
        } else if(key.equal("id")) {
            if(is_params) {
                this->noid = value.equal("false");
            } else {
                this->id = value;
                if(skip_body) return 1;
            }
        } else if(key.equal("params")) this->params = value;
        else if(key.equal("name")) {
            this->name.set(value);
            unescape(this->name);
        } else if(key.equal("fail_on_disconnect")) this->fail_on_disconnect = value.equal("true");
        else if(key.equal("info")) info = value;

        strip();
        a = next();
        if(a == ',') continue;
        if(a == '}') break;
        throw error::InvalidData();
    }

    return 0;
}

void JsonParser::strip() {
    while(index < buf.size()) {
        char a = buf.ptr()[index];
        if(a == ' ' || a == '\n' || a == '\r' || a == '\t') {
            index++;
            continue;
        }
        return;
    }
    throw error::InvalidData();
}

Slice JsonParser::read_object() {
    // 0-9, null, true, false
    int start = index;
    char a;
    bool is_number = true;
    for(;a = next();) {
        if(a == ' ' || a == ',' || a == '}' || a == '\n' || a == '\r') break;
        if(a < '0' || a > '9') is_number = false;
        if(!is_number && (index - start > 5)) throw error::InvalidData();
    }
    index--;
    Slice result(&buf.ptr()[start], index - start);
    if(!is_number) {
        if(!(result.equal("null") || result.equal("true") || result.equal("false"))) throw error::InvalidData();
    }
    return result;
}

Slice JsonParser::read_string() {
    if(next() != '"') throw error::InvalidData();
    int start = index;
    const char *ptr = buf.ptr();
    for(;index < buf.size();index++) {
        if(ptr[index] == '"' && ptr[index-1] != '\\') {
            index++;
            return Slice(&ptr[start], index - start - 1);
        }
    }
    throw error::InvalidData();
}

/* JData */

void JData::parse(ISlice data) {
    reset();
    if(!data.empty()) _data.set(data);
}

void JData::reset() {
    main_parsed = false;
    params_parsed = false;
    main.reset();
    params.reset();
    _name.clear();
    _data.clear();
}

Slice JData::get_id() {
    ensure_main();
    return main.id;
}

Slice JData::get_method() {
    ensure_main();
    return main.method;
}

Slice JData::get_name() {
    ensure_params();
    if(!_name.empty()) return _name;
    return params.name;
}

Slice JData::get_info() {
    ensure_params();
    return params.info;
}

bool JData::get_fail_on_disconnect() {
    ensure_params();
    return params.fail_on_disconnect;
}

bool JData::get_noid() {
    ensure_params();
    return params.noid;
}


void json::escape_string(ISlice &src, Buffer &dest) {
    dest.resize(src.size(), 0);
    int j = 0;
    for(int i=0;i<src.size();i++,j++) {
        char a = src.ptr()[i];
        if(j + 2 > dest.get_capacity()) dest.resize(j + 2);
        if(a == '\\') dest.ptr()[j++] = '\\';
        dest.ptr()[j] = a;
    }
    dest.resize(0, j);
}
