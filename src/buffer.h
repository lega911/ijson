
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

    bool equal(const char *s) {
        int i = 0;
        while(*s) {
            if(i >= _size) return false;
            if(*s != _ptr[i]) return false;
            i++;
            s++;
        }
        return i == _size;
    }

    int compare(ISlice &s) {
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

    inline bool operator==(const char *s) {return equal(s);}
    inline bool operator!=(const char *s) {return !equal(s);}
    inline bool empty() {return size() == 0;}
    inline bool valid() {return _ptr != NULL;}
    inline char* ptr() {return _ptr;}
    inline int size() {return _size;}
    std::string as_string(int _default=-1) {
        std::string s;
        if(valid()) {
            s.append(ptr(), size());
        } else {
            if(_default == 1) s.append("<empty>");
            else if(_default != 0) throw error::NoData();
        }
        return s;
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
    int atoi() {
        char *p = ptr();
        if(!_size) throw error::NoData();
        int value = 0, i = 0, n;
        bool negative = false;
        if(p[0] == '-') {
            negative = true;
            i = 1;
            if(_size < 2) throw error::InvalidData();
        }
        for(;i<_size;i++) {
            n = p[i] - '0';
            if(n < 0 || n > 9) throw error::InvalidData();
            value = value * 10 + n;
        }
        if(negative) return -value;
        return value;
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
    void add(ISlice &s) {
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
};
