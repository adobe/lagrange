/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
/**
 * IF no backend for fs is chosen, this file will either pick GHC_FS or STD_FS
 * For simplicity we don't boost filesystem in the auto pick
 */

// ==== 1st Attempt, see if you can use c++17 filesystem.
#if !defined(LAGRANGE_USE_STD_FS) && !defined(LAGRANGE_USE_GHC_FS) && \
    !defined(LAGRANGE_USE_BOOST_FS)
#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#define LAGRANGE_USE_STD_FS
#endif // has include
#endif // cpp 17
#endif // 1st attempt

// ==== 2nd Attempt; okay previous one failed, use gulrak.
#if !defined(LAGRANGE_USE_STD_FS) && !defined(LAGRANGE_USE_GHC_FS) && \
    !defined(LAGRANGE_USE_BOOST_FS)
#define LAGRANGE_USE_GHC_FS
#endif // 2nd attempt
