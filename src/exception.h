
#pragma once

#include <stddef.h>
#include <exception>

#define THROW(text) throw Exception(text, __FILE__, __LINE__, __PRETTY_FUNCTION__);

class Buffer;

class Exception : public std::exception {
private:
    const char *_reason;
    const char *_file;
    int _line;
    const char *_func;
    #ifdef DEBUG
        Buffer *_trace;
    #endif
public:
    Exception(const char *reason, const char *file=NULL, int line=0, const char *func=NULL);
    virtual const char* what() const noexcept {
        if(_reason) return _reason;
        return "Exception";
    };
    void print(const char *msg) const;
    #ifdef DEBUG
        ~Exception();
    #endif
};

namespace error {
    class NoData : public Exception {
    public:
        using Exception::Exception;
        NoData() : Exception("No data") {};
    };

    class InvalidData : public Exception {
    public:
        using Exception::Exception;
        InvalidData() : Exception("Invalid data") {};
    };
}

#ifdef DEBUG
    void fatal_error(int sig);
    void catch_fatal();
    void get_traceback(Buffer &r);
#endif
