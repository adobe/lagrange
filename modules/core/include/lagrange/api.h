#pragma once

#ifdef LA_CORE_STATIC_DEFINE
    #define LA_CORE_API
#else
    #ifndef LA_CORE_API
        #ifdef lagrange_core_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_CORE_API __declspec(dllexport)
            #else
                #define LA_CORE_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_CORE_API __declspec(dllimport)
            #else
                #define LA_CORE_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
