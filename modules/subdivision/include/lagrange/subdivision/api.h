#pragma once

#ifdef LA_SUBDIVISION_STATIC_DEFINE
    #define LA_SUBDIVISION_API
#else
    #ifndef LA_SUBDIVISION_API
        #ifdef lagrange_subdivision_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_SUBDIVISION_API __declspec(dllexport)
            #else
                #define LA_SUBDIVISION_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_SUBDIVISION_API __declspec(dllimport)
            #else
                #define LA_SUBDIVISION_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
