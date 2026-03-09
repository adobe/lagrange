#pragma once

#ifdef LA_REMESHING_IM_STATIC_DEFINE
    #define LA_REMESHING_IM_API
#else
    #ifndef LA_REMESHING_IM_API
        #ifdef lagrange_remeshing_im_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_REMESHING_IM_API __declspec(dllexport)
            #else
                #define LA_REMESHING_IM_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_REMESHING_IM_API __declspec(dllimport)
            #else
                #define LA_REMESHING_IM_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
