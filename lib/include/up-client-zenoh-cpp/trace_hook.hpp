#pragma once

#include <sys/sdt.h>
#include <boost/core/demangle.hpp>
#include <type_traits>
#include <string>
#include <iostream>
// #include <zenoh_commons.h>

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

template <typename T>
static std::string get_type(const T& t)
{
    return boost::core::demangle(typeid(t).name());
}

static std::ostream& operator<<(std::ostream& os, const z_query_t& arg)
{
    os << "z_query_t(" << arg._0 << ')';
    return os;
}
