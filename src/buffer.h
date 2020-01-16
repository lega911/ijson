
#pragma once

#include <string.h>
#include <iostream>
#include <stddef.h>
#include "memory.h"
#include "exception.h"


class Slice;
class Buffer;

class ISlice {
protected:
    char *_ptr = NULL;
    int _size = 0;
public:
    ISlice() {}

    bool starts_with(const char *s) {
        int i = 0;
        while(*s) {
            if(i >= _size) return false;
            if(*s != _ptr[i]) return false;
            i++;
            s++;
        }
        return true;
    }

    bool starts_with_lc(const char *s) {
        // lower case
        int i = 0;
        while(*s) {
            if(i >= _size) return false;
            char a = _ptr[i];
            if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if(*s != a) return false;
            i++;
            s++;
        }
        return true;
    }

    bool equal(const char *s) const {
        int i = 0;
        while(*s) {
            if(i >= _size) return false;
            if(*s != _ptr[i]) return false;
            i++;
            s++;
        }
        return i == _size;
    }

    bool equal(const ISlice &s) const {
        return compare(s) == 0;
    }

    int compare(const ISlice &s) const {
        int i = 0;
        int d;
        for(; i<size(); i++) {
            if(i >= s.size()) return 1;
            d = ptr()[i] - s.ptr()[i];
            if(d) return d;
        }
        if(i < s.size()) return -1;
        return 0;
    }

    inline bool operator==(const char *s) const {return equal(s);}
    inline bool operator!=(const char *s) const {return !equal(s);}
    inline bool operator==(const ISlice &s) const {return equal(s);}
    inline bool operator!=(const ISlice &s) const {return !equal(s);}
    inline bool empty() const {return size() == 0;}
    inline bool valid() const {return _ptr != NULL;}
    inline char* ptr() const {return _ptr;}
    inline int size() const {return _size;}
    inline operator bool() const {return size() != 0;}
    std::string as_string(int _default=-1) const {
        std::string s;
        if(valid()) {
            s.append(ptr(), size());
        } else {
            if(_default == 1) s.append("<empty>");
            else if(_default != 0) throw error::NoData();
        }
        return s;
    }
    i64 atoi() const {
        if(!size()) throw error::NoData();
        char *p = ptr();
        char *end = p + size();
        bool negative = false;
        if(*p == '-') {
            negative = true;
            if(size() < 2) throw error::InvalidData();
            p++;
        }
        i64 value = 0;
        for(;p < end; p++) {
            char a = *p - '0';
            if(a < 0 || a > 9) throw error::InvalidData();
            value = value * 10 + a;
        }
        return negative?-value:value;
    }
    i64 hextoi() const {
        if(!size()) throw error::NoData();
        i64 value = 0;
        char *p = ptr();
        char *end = p + size();
        for(;p < end; p++) {
           char a = *p;
           if(a >= '0' && a <= '9') value = (value << 4) + a - '0';
           else if(a >= 'A' && a <= 'F') value = (value << 4) + a - 'A' + 10;
           else if(a >= 'a' && a <= 'f') value = (value << 4) + a - 'a' + 10;
           else throw error::InvalidData();
        }
        return value;
    }
};


class Slice : public ISlice {
public:
    Slice() {}
    Slice(const char *ptr, int size) {
        _ptr = (char*)ptr;
        _size = size;
    }
    Slice(const char *ptr) {
        _ptr = (char*)ptr;
        _size = strlen(ptr);
    }
    Slice(ISlice &s) {
        set(s);
    }
    void set(const char *ptr, int size) {
        this->_ptr = (char*)ptr;
        _size = size;
    }
    void set(const char *ptr) {
        set(ptr, strlen(ptr));
    }
    void set(ISlice &s) {
        set(s.ptr(), s.size());
    }
    void reset() {
        set(NULL, 0);
    }

    Slice get(int len) {
        if(len > _size) THROW("Index error");
        return Slice(ptr(), len);
    }
    Slice pop(int len) {
        Slice r = get(len);
        _ptr += len;
        _size -= len;
        return r;
    }
    Slice split_left(char a) {
        for(int i=0;i<size();++i) {
            if(_ptr[i] != a) continue;
            Slice result = Slice(_ptr, i);
            _size -= i + 1;
            _ptr += i + 1;
            return result;
        }
        return pop(_size);
    }

    void remove(int len) {
        if(len > size()) len = size();
        _ptr += len;
        _size -= len;
    }

    Slice pop_line() {
        for(int i=0;i<size();i++) {
            if(_ptr[i] != '\n') continue;
            return pop(i + 1);
        }
        return Slice(NULL, 0);
    }
    void rstrip() {
        int i=_size-1;
        for(;i>=0;i--) {
            if(_ptr[i] == ' ' || _ptr[i] == '\n' || _ptr[i] == '\r') continue;
            break;
        }
        _size = i + 1;
    }
};


class Buffer : public ISlice {
protected:
    int _cap = 0;

    int _get_cap(int size) {
        int cap = 16;
        int step = 16;
        while(cap < size) {
            cap += step;
            if(step < 4096) {
                step *= 2;
            }
        }
        return cap;
    }
public:
    Buffer() : ISlice() {};
    Buffer(int size) : ISlice() {
        resize(size);
    };
    Buffer(const char *s) : ISlice() {
        add(s);
    };
    ~Buffer() {
        if(_ptr) _free(_ptr);
        _ptr = NULL;
    }

    void resize(int capacity) {
        if(_ptr == NULL) {
            _cap = _get_cap(capacity);
            _ptr = (char*)_malloc(_cap);
            if(_ptr == NULL) THROW("No memory");
        } else if(capacity > _cap) {
            _cap = _get_cap(capacity);
            char *p = (char*)_realloc(_ptr, _cap);
            if(!p) THROW("Realloc error");
            _ptr = p;
        }
    }
    void resize(int capacity, int size) {
        resize(capacity);
        _size = size;
    }
    inline int get_capacity() {return _cap;}

    void add(const char *buf, int size) {
        if(buf == NULL) throw error::NoData("Buffer.add buf == NULL");
        resize(_size + size);
        memmove(&_ptr[_size], buf, size);
        _size += size;
    }
    void add(const char *i) {
        while(*i != 0) {
            if(_size >= _cap) resize(_size + 1);
            _ptr[_size++] = *i;
            i++;
        }
    }
    void add(const ISlice &s) {
        add(s.ptr(), s.size());
    }
    void add_number(int n) {
        resize(_size + 12);
        
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
    void add_hex(u64 n) {
        if(!n) {
            add("0", 1);
            return;
        }
        const char hex[] = "0123456789abcdef";
        char r[16];
        int i = 15;
        while(n) {
            r[i--] = hex[n & 0xf];
            n >>= 4;
        }
        add(&r[i + 1], 15-i);
    };
    void set(const char *buf, int size) {
        clear();
        add(buf, size);
    }
    void set(const char *str) {
        clear();
        add(str);
    }
    void set(ISlice &s) {
        set(s.ptr(), s.size());
    }
    void clear() {
        _size = 0;
    }
    void remove_left(int n) {
        if(n <= 0) return;
        if(_size <= n) {
            _size = 0;
        } else {
            _size -= n;
            memmove(_ptr, &_ptr[n], _size);
        }
    }
    void move(Buffer *buf) {
        std::swap(_cap, buf->_cap);
        std::swap(_ptr, buf->_ptr);
        _size = buf->_size;
        buf->_size = 0;
    }
};
