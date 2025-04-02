/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#ifdef LA_IMAGE_IO_STATIC_DEFINE
    #define LA_IMAGE_IO_API
#else
    #ifndef LA_IMAGE_IO_API
        #ifdef lagrange_image_io_EXPORTS
            // We are building this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_IMAGE_IO_API __declspec(dllexport)
            #else
                #define LA_IMAGE_IO_API __attribute__((visibility("default")))
            #endif
        #else
            // We are using this library
            #if defined(_WIN32) || defined(_WIN64)
                #define LA_IMAGE_IO_API __declspec(dllimport)
            #else
                #define LA_IMAGE_IO_API __attribute__((visibility("default")))
            #endif
        #endif
    #endif
#endif
