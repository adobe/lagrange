#pragma once

#ifdef LA_IO_STATIC_DEFINE
    #define LA_IO_API
#else
    #ifndef LA_IO_API
        #ifdef lagrange_io_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_IO_API __declspec(dllexport)
            #else
                #define LA_IO_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_IO_API __declspec(dllimport)
            #else
                #define LA_IO_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
