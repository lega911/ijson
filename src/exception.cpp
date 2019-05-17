
#include "exception.h"


#ifdef DEBUG
    #include <signal.h>
    #include <iostream>

    #ifndef ALPINE
        #include <execinfo.h>
        #include <unistd.h>
    #endif

    void fatal_error(int sig) {
        void *array[16];
        std::cerr << "Error: signal " << sig << std::endl;
        #ifndef ALPINE
            backtrace_symbols_fd(array, backtrace(array, 16), STDERR_FILENO);
        #endif
        exit(1);
    }

    void catch_fatal() {
        signal(SIGSEGV, fatal_error);
    };
#endif
