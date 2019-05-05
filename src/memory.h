
#ifndef MEMORY_H
#define MEMORY_H

#include "stdint.h"

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;


#ifdef DEBUG
    i32 get_memory_allocated();
    void *_malloc(u32 size);
    void *_realloc(void *ptr, u32 size);
    void _free(void *ptr);
#else
    #define _malloc malloc
    #define _realloc realloc
    #define _free free
#endif

#endif
