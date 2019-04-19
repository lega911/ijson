
#include <iostream>
#include "memory.h"


#ifdef DEBUG
    i32 count = 0;
    i32 get_memory_allocated() {
        return count;
    }

    void *_malloc(u32 size) {
        count++;
        void *ptr = malloc(size);
        //std::cout << "[" << count << "] + " << size << "b " << ptr << std::endl;
        return ptr;
    }
    void *_realloc(void *ptr, u32 size) {
        ptr = realloc(ptr, size);
        //std::cout << "[" << count << "] -+ " << size << "b " << ptr << std::endl;
        return ptr;
    }
    void _free(void *ptr) {
        count--;
        //std::cout << "[" << count << "] - " << ptr << std::endl;
        free(ptr);
    }

    void * operator new(std::size_t n) throw(std::bad_alloc)
    {
        return _malloc(n);
    }
    void operator delete(void *p) throw()
    {
        _free(p);
    }
#endif
