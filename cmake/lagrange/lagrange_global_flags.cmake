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
include_guard(GLOBAL)

# options
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Reduce the warning level of external files to the selected value (W1 - only major).
    # Requires Visual Studio 2017 version 15.7
    # https://blogs.msdn.microsoft.com/vcblog/2017/12/13/broken-warnings-theory/

    # There is an issue in using these flags in earlier versions of MSVC:
    # https://developercommunity.visualstudio.com/content/problem/220812/experimentalexternal-generates-a-lot-of-c4193-warn.html
    if(MSVC_VERSION GREATER 1920)
        add_compile_options(/experimental:external)
        add_compile_options(/external:W1)
    endif()

    # When building in parallel, MSVC sometimes fails with the following error:
    # > fatal error C1090: PDB API call failed, error code '23'
    # To avoid this problem, we force PDB write to be synchronous with /FS.
    # https://developercommunity.visualstudio.com/content/problem/48897/c1090-pdb-api-call-failed-error-code-23.html
    add_compile_options(/FS)

    # Boost::hana requires /EHsc, so we need to enable it globally
    include(lagrange_filter_flags)
    set(LAGRANGE_GLOBAL_FLAGS
        /EHsc # Compatibility with Boost::hana
    )
    lagrange_filter_flags(LAGRANGE_GLOBAL_FLAGS)
    message(STATUS "Adding global flags: ${LAGRANGE_GLOBAL_FLAGS}")
    add_compile_options(${LAGRANGE_GLOBAL_FLAGS})
else()
    include(lagrange_filter_flags)
    set(LAGRANGE_GLOBAL_FLAGS
        -fdiagnostics-color=always # GCC
        -fcolor-diagnostics # Clang
    )
    lagrange_filter_flags(LAGRANGE_GLOBAL_FLAGS)
    message(STATUS "Adding global flags: ${LAGRANGE_GLOBAL_FLAGS}")
    add_compile_options(${LAGRANGE_GLOBAL_FLAGS})
endif()

if(LAGRANGE_WITH_TRACY)
    include(lagrange_filter_flags)
    set(LAGRANGE_GLOBAL_FLAGS
        "-fno-omit-frame-pointer"
        "-g"
    )
    lagrange_filter_flags(LAGRANGE_GLOBAL_FLAGS)
    message(STATUS "Adding global flags: ${LAGRANGE_GLOBAL_FLAGS}")
    add_compile_options(${LAGRANGE_GLOBAL_FLAGS})
endif()

if(EMSCRIPTEN)
    # Use "-s ALLOW_MEMORY_GROWTH=1" to allow the WASM heap to grow.
    # https://github.com/emscripten-core/emscripten/blob/38e16464cffc8f886364fe4919a712131a4c9456/src/settings.js#L217
    add_link_options(-sALLOW_MEMORY_GROWTH=1)

    # Use "-pthread" to allow multi-threading. See https://emscripten.org/docs/porting/pthreads.html.
    if(LAGRANGE_USE_WASM_THREADS)
        # Use "-s PROXY_TO_PTHREAD=1" to move program execution to a worker thread, leaving the
        # main thread available to respond to requests for more worker threads. Without this
        # flag, we'd need to prepopulate a thread pool with enough threads for every example app
        # (with something like "-s PTHREAD_POOL_SIZE=40"), otherwise the main thread would block
        # as soon another thread is launched.
        # https://github.com/emscripten-core/emscripten/blob/38e16464cffc8f886364fe4919a712131a4c9456/src/settings.js#L1175.
        set(EMSCRIPTEN_THREAD_FLAG "-pthread")
        add_link_options(-sPROXY_TO_PTHREAD=1)

        # Use "-Wno-pthreads-mem-growth" to silence the warning "USE_PTHREADS + ALLOW_MEMORY_GROWTH
        # may run non-wasm code slowly, see https://github.com/WebAssembly/design/issues/1271".
        # Examples don't run much (if any) non-wasm code.
        add_link_options(-Wno-pthreads-mem-growth)

        # Set PTHREAD_POOL_SIZE parameter. Ideally we would use 'navigator.hardwareConcurrency',
        # but this requires NodeJS >= 21. Currently, emsdk (4.0.8) only ships with NodeJS 20.
        # https://developer.mozilla.org/en-US/docs/Web/API/Navigator/hardwareConcurrency
        #
        # Note that PTHREAD_POOL_SIZE sets the upper limit for concurrency, so we use the number of
        # logical cores on the host machine when guessing via CMake.
        #
        # Related OneTBB/Emscripten issues:
        # https://github.com/emscripten-core/emscripten/discussions/21963
        # https://github.com/uxlfoundation/oneTBB/issues/1287
        # https://github.com/uxlfoundation/oneTBB/blob/master/WASM_Support.md#limitations
        if(NOT DEFINED PTHREAD_POOL_SIZE)
            find_program(NODE_EXECUTABLE node REQUIRED)
            execute_process(COMMAND ${NODE_EXECUTABLE} --version OUTPUT_VARIABLE NODE_VERSION)
            string(REPLACE "v" "" NODE_VERSION ${NODE_VERSION})
            if(NODE_VERSION VERSION_LESS 21.0.0)
                message(WARNING "navigator.hardwareConcurrency requires NodeJS >= 21.0.0.")
                cmake_host_system_information(RESULT NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
                set(PTHREAD_POOL_SIZE ${NUMBER_OF_LOGICAL_CORES} CACHE STRING "Number of threads in the thread pool")
            else()
                set(PTHREAD_POOL_SIZE navigator.hardwareConcurrency CACHE STRING "Number of threads in the thread pool")
            endif()
        endif()
        cmake_host_system_information(RESULT NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
        if(PTHREAD_POOL_SIZE LESS NUMBER_OF_LOGICAL_CORES)
            message(WARNING "PTHREAD_POOL_SIZE (${PTHREAD_POOL_SIZE}) is smaller than the number of logical cores (${NUMBER_OF_LOGICAL_CORES}).")
        endif()
        message(STATUS "Setting PTHREAD_POOL_SIZE to ${PTHREAD_POOL_SIZE}")
        add_link_options("SHELL:-s PTHREAD_POOL_SIZE=${PTHREAD_POOL_SIZE}")
    else()
        set(EMSCRIPTEN_THREAD_FLAG "")
    endif()

    # Use the "-fexceptions" flag to allow C++ code to catch C++ exceptions after compilation to
    # WebAssembly. At some point, when more WASM engines support exceptions natively, change
    # "-fexceptions" to "-fwasm-exceptions". See https://emscripten.org/docs/porting/exceptions.html.
    if(LAGRANGE_USE_WASM_EXCEPTIONS)
        set(EMSCRIPTEN_EXCEPTION_HANDLER_FLAG "-fwasm-exceptions")
    else()
        set(EMSCRIPTEN_EXCEPTION_HANDLER_FLAG "-fexceptions") # "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
    endif()

    add_compile_options(${EMSCRIPTEN_EXCEPTION_HANDLER_FLAG} ${EMSCRIPTEN_THREAD_FLAG})
    add_link_options(${EMSCRIPTEN_EXCEPTION_HANDLER_FLAG} ${EMSCRIPTEN_THREAD_FLAG})

    # Generate a source map using LLVM debug information.
    # https://emscripten.org/docs/tools_reference/emcc.html#emcc-gsource-map
    #
    # I am still figuring out how to use debugging tools with wasm code. Here are two useful links for this:
    # https://developer.chrome.com/docs/devtools/wasm
    # https://floooh.github.io/2023/11/11/emscripten-ide.html
    add_compile_options($<$<NOT:$<CONFIG:Release>>:-gsource-map>)

    # For LTO, Emscripten recommends full/regular LTO (https://emscripten.org/docs/compiling/WebAssembly.html#backends)
    # But CMake will currently default to thin lto when enabling INTERPROCEDURAL_OPTIMIZATION through cmake (https://gitlab.kitware.com/cmake/cmake/-/issues/23136)
    # So we pass -flto manually for now.
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

    if(LAGRANGE_WASM_FAST_LINK)
        # If we want faster link time, only optimize up to -O1, which avoids running binaryen optimization.
        # https://github.com/emscripten-core/emscripten/issues/17019
        add_link_options($<$<NOT:$<CONFIG:Debug>>:-O1>)
        add_link_options($<$<NOT:$<CONFIG:Debug>>:-sERROR_ON_WASM_CHANGES_AFTER_LINK>)
    else()
        # If we want faster runtime, let's enable Full LTO.
        add_compile_options($<$<NOT:$<CONFIG:Debug>>:-flto>)
        add_link_options($<$<NOT:$<CONFIG:Debug>>:-flto>)
        add_link_options($<$<CONFIG:RelWithDebInfo>:-Wno-limited-postlink-optimizations>)
    endif()
endif()
