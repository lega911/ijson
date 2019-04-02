
#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stddef.h>

class Exception {
public:
    static const int NOT_IMPLEMENTED = 7;
    static const int DATA_ERROR = 21;
    static const int NO_DATA = 22;

    int code;
    const char *msg;
    Exception() : code(0), msg(NULL) {};
    Exception(int code) : code(code), msg(NULL) {};
    Exception(int code, const char *msg) : code(code), msg(msg) {};
    const char *get_msg() {
        if(msg) return msg;
        if(code == DATA_ERROR) return "Data error";
        else if(code == NOT_IMPLEMENTED) return "Not implemented error";
        return "Unknown error";
    }
};

namespace error {
    Exception not_implemented(const char *msg);
}

#endif /* EXCEPTION_H */
