#pragma once

#ifdef LA_FS_STATIC_DEFINE
    #define LA_FS_API
#else
    #ifndef LA_FS_API
        #ifdef lagrange_fs_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_FS_API __declspec(dllexport)
            #else
                #define LA_FS_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_FS_API __declspec(dllimport)
            #else
                #define LA_FS_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
