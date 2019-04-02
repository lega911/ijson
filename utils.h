
#ifndef UTILS_H
#define UTILS_H

#define _OPEN_SYS_ITOA_EXT

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>

#include "exception.h"


class Slice;
class Buffer;

long get_time();
void print2(const char *title, const char *s, int size);
void print2(const char *title, Buffer &b);
void print2(const char *title);


class Buffer {
private:
    int p_size;
    int p_len;
    char *data;
public:
    Buffer() {
        p_size = 0;
        p_len = 0;
        data = NULL;
    };
    Buffer(int size) {
        p_size = 0;
        p_len = 0;
        data = NULL;
        resize(size);
    };
    ~Buffer() {
        if(data) free(data);
    }
    void resize(int size) {
        if(data == NULL) {
            p_len = 0;
            p_size = size;
            data = (char*)malloc(p_size);
        } else if(size > p_size) {
            p_size = size;
            data = (char*)realloc(data, p_size);
        }
    }
    void resize(int size, int len) {
        resize(size);
        p_len = len;
    }
    void add(const char *buf, int size) {
        resize(p_len + size);
        memcpy(&data[p_len], buf, size);
        p_len += size;
    }
    void add(Slice &s);
    void add(const char *s) {
        add(s, strlen(s));
    }
    void add(Buffer *b) {
        add(b->ptr(), b->size());
    }
    void add_number(int n) {
        resize(p_len + 12);
        
        char s[12];
        int d;
        int i=11;
        bool negative = false;
        if(n == 0) {
            s[i--] = '0';
        } else if(n < 0) {
            n = -n;
            negative = true;
        }
        while(n) {
            d = n % 10;
            s[i--] = d + '0';
            n = n / 10;
        }
        if(negative) {
            s[i--] = '-';
        }
        add(&s[i + 1], 11 - i);
    }
    void set(const char *buf, int size) {
        clear();
        add(buf, size);
    }
    void set(Slice &s);
    void clear() {
        p_len = 0;
    }
    void remove_left(int n) {
        if(n <= 0) return;
        if(p_len <= n) {
            p_len = 0;
        } else {
            p_len -= n;
            memcpy(&data[n], data, p_len);
        }
    }
    char *ptr() {return data;}
    int size() {return p_len;}
    bool empty() {return p_len == 0;}
    Slice slice();
    std::string as_str() {
        std::string s;
        s.append(ptr(), size());
        return s;
    }
};

class Slice {
private:
    char *_ptr;
    int p_size;
public:
    Slice() {
        _ptr = NULL;
        p_size = 0;
    }
    Slice(const char *ptr, int size) {
        _ptr = (char*)ptr;
        p_size = size;
    }
    ~Slice() {
        _ptr=NULL;
    }
    void set(const char *ptr, int size) {
        this->_ptr = (char*)ptr;
        p_size = size;
    }
    void clear() {
        set(NULL, 0);
    }

    Slice get(int len) {
        if(len > p_size) throw "slice: len error";
        return Slice(ptr(), len);
    }
    Slice pop(int len) {
        Slice r = get(len);
        _ptr += len;
        p_size -= len;
        return r;
    }
    
    void remove(int len) {
        if(len > size()) len = size();
        _ptr += len;
        p_size -= len;
    }
    
    bool starts_with(const char *s) {
        if(s[0] == 0) return true;
        if(size() == 0) return false;
        if(s[0] != ptr()[0]) return false;

        int len = strlen(s);
        if(len > p_size) return false;
        return memcmp(ptr(), s, len) == 0;
    }
    
    bool equal(const char *s) {
        int len = strlen(s);
        if(len != p_size) return false;
        return memcmp(s, _ptr, p_size) == 0;
    }
        
    Slice pop_line() {
        for(int i=0;i<size();i++) {
            if(_ptr[i] != '\n') continue;
            return pop(i + 1);
        }
        return Slice(NULL, 0);
    }
    void rstrip() {
        int i=p_size-1;
        for(;i>=0;i--) {
            if(_ptr[i] == ' ' || _ptr[i] == '\n' || _ptr[i] == '\r') continue;
            break;
        }
        p_size = i + 1;
    }
    
    inline bool empty() {return size() == 0;}
    inline bool valid() {return _ptr != NULL;}
    inline char* ptr() {return _ptr;}
    inline int size() {return p_size;}
    std::string as_string() {
        return as_string(5);
    }
    std::string as_string(int _default) {
        std::string s;
        if(valid()) {
            s.append(ptr(), size());
        } else {
            if(_default == 1) s.append("<empty>");
            else if(_default != 0) throw Exception(Exception::NO_DATA, "Not valid slice");
        }
        return s;
    }
};

#endif /* UTILS_H */

