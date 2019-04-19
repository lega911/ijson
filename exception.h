
#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stddef.h>
#include <exception>


class Exception : public std::exception {
private:
    const char *_reason;
public:
    static const char *message;
    Exception() {};
    Exception(const char *reason) : _reason(reason) {};
    virtual const char* what() const noexcept {
        if(_reason) return _reason;
        return "Exception";
    };
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

#endif /* EXCEPTION_H */
