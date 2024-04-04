#pragma once

#ifdef LA_UI_STATIC_DEFINE
    #define LA_UI_API
#else
    #ifndef LA_UI_API
        #ifdef lagrange_ui_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_UI_API __declspec(dllexport)
            #else
                #define LA_UI_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_UI_API __declspec(dllimport)
            #else
                #define LA_UI_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
