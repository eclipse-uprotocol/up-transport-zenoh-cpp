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
    }

#define LINENO() __LINE__
#define TRACE() DTRACE_PROBE3(testRpcClient, LINENO(), __LINE__, __FUNCTION__, file_name())

#else

#define IMPL_TRACEHOOK()
#define TRACE()

#endif