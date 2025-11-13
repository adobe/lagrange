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
#include <lagrange/Logger.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/fpe.h>

#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#ifdef LAGRANGE_WITH_CPPTRACE
    #include <cpptrace/from_current.hpp>
#endif

#if LAGRANGE_TARGET_OS(WASM)
    #include <emscripten.h>
#endif

#include <cstdlib>

namespace {

int wrapped_run(Catch::Session& session)
{
#if LAGRANGE_TARGET_OS(WASM)
    // Remove this if/when cpptrace start supporting Emscripten:
    // https://github.com/jeremy-rifkin/cpptrace/issues/175
    try {
        return session.run();
    } catch (const std::exception& e) {
        lagrange::logger().critical("Exception: {}", e.what());

        char callstack[4096];
        emscripten_get_callstack(EM_LOG_C_STACK, callstack, sizeof(callstack));
        std::puts(callstack);
        return EXIT_FAILURE;
    }
#elif LAGRANGE_WITH_CPPTRACE
    int ret = EXIT_FAILURE;
    CPPTRACE_TRY
    {
        ret = session.run();
    }
    CPPTRACE_CATCH(const std::exception& e)
    {
        lagrange::logger().critical("Exception: {}", e.what());
    #if LAGRANGE_TARGET_FEATURE(RTTI)
        cpptrace::from_current_exception().print();
    #endif
        ret = EXIT_FAILURE;
    }
    return ret;
#else
    return session.run();
#endif
}

#if LAGRANGE_TARGET_OS(WASM) && !defined(__EMSCRIPTEN_PTHREADS__)

static_assert(false, "Emscripten must be compiled with pthreads support");

#endif

} // namespace

int main(int argc, char* argv[])
{
    Catch::Session session;
    int log_level = spdlog::level::warn;
    bool fpe_flag = false;

    auto cli = session.cli() |
               Catch::Clara::Opt(log_level, "log level")["-l"]["--log-level"]("Log level") |
               Catch::Clara::Opt(fpe_flag, "use fpe")["--enable-fpe"]("Enable FPE");

    session.cli(cli);

    if (auto ret = session.applyCommandLine(argc, argv)) {
        return ret;
    }

    log_level = std::max(0, std::min(6, log_level));
    lagrange::logger().set_level(static_cast<spdlog::level::level_enum>(log_level));

    if (fpe_flag) {
        lagrange::logger().info("Enabling floating point exceptions");
        lagrange::enable_fpe();
    }

    return wrapped_run(session);
}
