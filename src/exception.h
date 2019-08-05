
#pragma once

#include <stddef.h>
#include <exception>


class Buffer;

class Exception : public std::exception {
private:
    const char *_reason;
    #ifdef DEBUG
        Buffer *_trace;
    #endif
public:
    Exception();
    Exception(const char *reason);
    virtual const char* what() const noexcept {
        if(_reason) return _reason;
        return "Exception";
    };
    const char* trace() const noexcept;
    #ifdef DEBUG
        ~Exception();
    #endif
};

namespace error {
    class NotImplemented : public Exception {
    public:
        using Exception::Exception;
        NotImplemented() : Exception("Not implemented") {};
    };

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

    class OutOfIndex : public Exception {
    public:
        using Exception::Exception;
        OutOfIndex() : Exception("Out of index") {};
    };

    class ArgumentError : public Exception {
    public:
        using Exception::Exception;
        ArgumentError() : Exception("Argument error") {};
    };

    class NoMemory : public Exception {
    public:
        using Exception::Exception;
        NoMemory() : Exception("No memory") {};
    };
}

#ifdef DEBUG
    // ./ijson.debug 2>&1 | c++filt
    void fatal_error(int sig);
    void catch_fatal();
    void get_traceback(Buffer &r);
#endif
