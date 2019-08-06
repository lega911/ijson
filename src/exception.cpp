
#include "exception.h"
#include "buffer.h"
#include "utils.h"


#ifdef DEBUG
    Exception::Exception(const char *reason, const char *file, int line, const char *func) : _reason(reason), _file(file), _line(line), _func(func) {
        _trace = new Buffer();
        get_traceback(*_trace);
    };

    void Exception::print(const char *msg) const {
        std::cout << ltime() << msg << " '" << _reason << "'";
        if(_file) std::cout << ", file " << _file << ":" << _line << " " << _func << std::endl;
        else std::cout << std::endl;
        std::cout << _trace->ptr();
    }

    Exception::~Exception() {
        delete _trace;
    }
#else
    Exception::Exception(const char *reason, const char *file, int line, const char *func) : _reason(reason), _file(file), _line(line), _func(func) {};
    void Exception::print(const char *msg) const {
        std::cout << ltime() << msg << " '" << _reason << "'";
        if(_file) std::cout << ", file " << _file << ":" << _line << " " << _func << std::endl;
        else std::cout << std::endl;
    }
#endif


#ifdef DEBUG
    #include <signal.h>
    #include <iostream>
    #include <execinfo.h>
    #include <unistd.h>
    #include <cxxabi.h>
    #include "buffer.h"

    void fatal_error(int sig) {
        Buffer trace;
        get_traceback(trace);
        std::cerr << "Error: signal " << sig << std::endl;
        std::cerr << trace.ptr() << std::endl;
        exit(1);
    }

    void catch_fatal() {
        signal(SIGSEGV, fatal_error);
    };

    void get_traceback(Buffer &r) {
        void *addr[32];
        char **names;

        int n = backtrace(addr, 32);
        names = backtrace_symbols(addr, n);
        if(!names) return;

        size_t funcsize = 256;
        char *func = (char*)malloc(funcsize);

        Buffer bname(256);
        for(int i = 1; i<n; i++) {
            Slice line(names[i]);
            Slice file = line.split_left('(');
            Slice _name = line.split_left('+');
            line = line.split_left(')');
            bname.set(_name);
            bname.add("\0", 1);

            int status;
	        char* ret = abi::__cxa_demangle(bname.ptr(), func, &funcsize, &status);
            if(status == 0) {
                func = ret;
                r.add(file);
                r.add(": ");
                r.add(func);
                r.add(" +");
                r.add(line);
            } else {
                r.add((char*)names[i]);
            }
            r.add("\n");
        }
        r.add("\0", 1);
        free(func);
        free(names);
    }
#endif
