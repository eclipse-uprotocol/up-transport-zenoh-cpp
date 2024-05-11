#pragma once

#include <sys/sdt.h>

#if 1

#define IMPL_TRACEHOOK() \
    static const char* file_name() \
    { \
        static const char* full_ptr = __FILE__; \
        static const char* ptr = nullptr; \
        if (ptr == nullptr) { \
            ptr = full_ptr; \
            for (size_t i = sizeof(__FILE__) - 1; i > 0; i--) { \
                if (full_ptr[i] == '/') { \
                    ptr = full_ptr + i + 1; \
                    break; \
                } \
            } \
        } \
        return ptr; \
    } \
    \
    static void name_combine(char buf[64], int line_no, const char* func, const char* desc) { snprintf(buf, 64, "%s:%d:%s:%s", file_name(), line_no, func, desc); }

#define LINENO() __LINE__
#define TRACE(desc) { char buf[64]; name_combine(buf, __LINE__, __FUNCTION__, desc); DTRACE_PROBE1(tracehook, LINENO(), buf); }

#else

#define IMPL_TRACEHOOK()
#define TRACE(desc)

#endif