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
#include <lagrange/utils/fpe.h>

#include <lagrange/utils/build.h>

#if defined(LA_DISABLE_FPE)

namespace lagrange {

void enable_fpe() {}

void disable_fpe() {}

} // namespace lagrange

#elif LAGRANGE_TARGET_OS(WINDOWS)

    #include <float.h>
    #include <stdio.h>
    #include <exception>
    #pragma fenv_access(on)

namespace lagrange {

void enable_fpe()
{
    unsigned int enable_bits = _EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW;

    // Clear any pending FP exceptions. This must be done
    // prior to enabling FP exceptions since otherwise there
    // may be a "deferred crash" as soon the exceptions are
    // enabled.
    _clearfp();

    // Zero out the specified bits, leaving other bits alone.
    _controlfp_s(0, ~enable_bits, enable_bits);
}

void disable_fpe()
{
    // Set all of the exception flags, which suppresses FP
    // exceptions on the x87 and SSE units.
    _controlfp_s(0, _MCW_EM, _MCW_EM);
}

} // namespace lagrange

#elif LAGRANGE_TARGET_OS(LINUX)

    #include <fenv.h>

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

#elif LAGRANGE_TARGET_OS(APPLE)

    #include <fenv.h>

    #if LAGRANGE_TARGET_PLATFORM(x86_64)

namespace lagrange {

// Source: http://www-personal.umich.edu/~williams/archive/computation/fe-handling-example.c
// SPDX-License-Identifier: CC-PDDC
//
// Public domain polyfill for feenableexcept on OS X
//
// These functions have been modified by Adobe.
//
// All modifications are Copyright 2020 Adobe.

void enable_fpe()
{
    unsigned int excepts = FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW;

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

void disable_fpe()
{
    unsigned int excepts = FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW;

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

} // namespace lagrange

    #elif LAGRANGE_TARGET_PLATFORM(arm64)

        #include <fenv.h>
        #include <math.h>
        #include <signal.h>
        #include <stdio.h>
        #include <stdlib.h>

namespace lagrange {

static void fpe_signal_action([[maybe_unused]] int sig, siginfo_t* sip, [[maybe_unused]] void* scp)
{
    /* see signal.h for codes */
    int fe_code = sip->si_code;
    if (fe_code == ILL_ILLTRP) {
        printf("Illegal trap detected\n");
    } else {
        printf("Code detected : %d\n", fe_code);
    }

    int i = fetestexcept(FE_INVALID);
    if (i & FE_INVALID) {
        printf("---> Invalid valued detected\n");
    }

    abort();
}

void enable_fpe()
{
    fenv_t env;
    fegetenv(&env);
    env.__fpcr = env.__fpcr | __fpcr_trap_invalid;
    fesetenv(&env);

    struct sigaction act;
    act.sa_sigaction = fpe_signal_action;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &act, NULL);
}

void disable_fpe()
{
    // Not implemented.
}

} // namespace lagrange

    #else

        #pragma error("Unsupported platform")

    #endif

#else

    #pragma error("Unsupported OS")

#endif
