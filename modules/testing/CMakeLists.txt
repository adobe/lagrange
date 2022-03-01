#
# Copyright 2020 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# 1. define module
lagrange_add_module()
set_target_properties(lagrange_testing PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

# 2. dependencies
lagrange_include_modules(io)
target_link_libraries(lagrange_testing PUBLIC
    Catch2::Catch2
    Threads::Threads
    lagrange::io
)

# 3. test-specific properties
lagrange_download_data()
add_dependencies(lagrange_testing lagrange_download_data)

if(EMSCRIPTEN)
    # Use "-s ALLOW_MEMORY_GROWTH=1" to allow the WASM heap to grow.

    # Use "-s PROXY_TO_PTHREAD=1" to move program execution to a worker thread, leaving the main
    # thread available to respond to requests for more worker threads. Without this flag, we'd need
    # to prepopulate a thread pool with enough threads for every unit test (with something like
    # "-s PTHREAD_POOL_SIZE=40"), otherwise the main thread would block as soon another thread is
    # launched. See https://emscripten.org/docs/porting/pthreads.html#additional-flags and
    # https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L1019.

    # Use "-s EXIT_RUNTIME=1" to exit the Node.js process when the main thread completes. Otherwise,
    # any worker threads (even completed threads) will keep the process alive. See
    # https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L91.

    # Use "-s NODERAWFS=1" to allow tests running on Node.js to access the system's files (through
    # Emscripten's "raw filesystem" backend). This is used by several unit tests to read test data.
    # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L898.

    # Use "-Wno-pthreads-mem-growth" to silence the warning "USE_PTHREADS + ALLOW_MEMORY_GROWTH may
    # run non-wasm code slowly, see https://github.com/WebAssembly/design/issues/1271". Unit tests
    # don't run much (if any) non-wasm code.

    target_link_options(lagrange_testing INTERFACE
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s PROXY_TO_PTHREAD=1"
        "SHELL:-s EXIT_RUNTIME=1"
        "SHELL:-s NODERAWFS=1"
        -Wno-pthreads-mem-growth
    )
endif()

target_compile_definitions(lagrange_testing
    PUBLIC
        CATCH_CONFIG_ENABLE_BENCHMARKING
    PRIVATE
        TEST_DATA_DIR="${LAGRANGE_DATA_FOLDER}"
)

if(LAGRANGE_USE_PCH)
    add_library(lagrange_testing_pch INTERFACE)
    target_link_libraries(lagrange_testing_pch INTERFACE lagrange_core_pch)
    target_precompile_headers(lagrange_testing_pch INTERFACE
        <lagrange/testing/common.h>
    )
endif()

# Set flag to hide unit tests depending on private data availability
if(LAGRANGE_EXTERNAL_ONLY)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[.corp]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_CORP_FLAG=\"[corp]\")
endif()

# CMake option to enable slow unit tests in ctest
option(LAGRANGE_SLOW_TESTS "Enable slow unit tests" OFF)
if(LAGRANGE_SLOW_TESTS)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_FLAG=\"[slow]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_FLAG=\"[.slow]\")
endif()

# When slow tests are disabled, we still hide some tests in Debug only
if(LAGRANGE_SLOW_TESTS)
    target_compile_definitions(lagrange_testing PUBLIC -DLA_SLOW_DEBUG_FLAG=\"[slow-debug]\")
else()
    target_compile_definitions(lagrange_testing PUBLIC
        LA_SLOW_DEBUG_FLAG=$<IF:$<CONFIG:Debug>,\"[.slow-debug]\",\"[slow-debug]\">
    )
endif()
