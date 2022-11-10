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
#if defined(_WIN32) || defined(LA_DISABLE_FPE)

namespace {

#define FE_DIVBYZERO 0
#define FE_INVALID 0
#define FE_OVERFLOW 0

inline int feenableexcept(unsigned int /*excepts*/)
{
    return 0;
}

inline int fedisableexcept(unsigned int /*excepts*/)
{
    return 0;
}

} // namespace

#elif defined(__linux__)

#include <fenv.h>

#elif defined(__APPLE__)

#include <fenv.h>

// Source: http://www-personal.umich.edu/~williams/archive/computation/fe-handling-example.c
// SPDX-License-Identifier: CC-PDDC
//
// Public domain polyfill for feenableexcept on OS X
//
// These functions have been modified by Adobe.
//
// All modifications are Copyright 2020 Adobe.

namespace {

inline int feenableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    // previous masks
    unsigned int old_excepts;

    if (fegetenv(&fenv)) {
        return -1;
    }
    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // unmask
    fenv.__control &= ~new_excepts;
    fenv.__mxcsr &= ~(new_excepts << 7);

    return fesetenv(&fenv) ? -1 : old_excepts;
}

inline int fedisableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    // all previous masks
    unsigned int old_excepts;

    if (fegetenv(&fenv)) {
        return -1;
    }
    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // mask
    fenv.__control |= new_excepts;
    fenv.__mxcsr |= new_excepts << 7;

    return fesetenv(&fenv) ? -1 : old_excepts;
}

} // namespace

#else

#pragma error("Unsupported platform")

#endif

////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

void enable_fpe()
{
    feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
}

void disable_fpe()
{
    fedisableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
}

} // namespace lagrange
