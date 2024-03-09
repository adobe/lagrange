#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
function(lagrange_limit_parallelism)
    # Query system information
    cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
    cmake_host_system_information(RESULT AVAILABLE_PHYSICAL_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)
    cmake_host_system_information(RESULT AVAILABLE_VIRTUAL_MEMORY QUERY AVAILABLE_VIRTUAL_MEMORY)
    cmake_host_system_information(RESULT TOTAL_VIRTUAL_MEMORY QUERY TOTAL_VIRTUAL_MEMORY)
    cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)

    # Peak memory computed "manually" for each platform (in MB)
    # Use a hard coded limit of 2 parallel linking jobs
    set(max_rss_linux_debug 3000)
    set(max_rss_linux_release 6000) # Force -j3 on a 16G memory machine.
    set(max_rss_darwin_debug 729)
    set(max_rss_darwin_release 395)
    set(max_rss_windows_debug 2100)
    set(max_rss_windows_release 1300)

    # Use "release" limit only for matching single-config mode
    if(GENERATOR_IS_MULTI_CONFIG OR NOT DEFINED CMAKE_BUILD_TYPE)
        message(STATUS "Defaulting to debug")
        set(_postfix "debug")
    else()
        string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
        if(_type STREQUAL release)
            set(_postfix "release")
        else()
            set(_postfix "debug")
        endif()
    endif()

    string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} _system)

    # Use a 3/2 factor safety margin compared to observed memory usage
    math(EXPR num_cpu_memory "${AVAILABLE_PHYSICAL_MEMORY} * 3 / 2 / ${max_rss_${_system}_${_postfix}}")

    # Compute limits for link/compile steps
    set(num_cpu_link 2)
    set(num_cpu_compile ${NUMBER_OF_PHYSICAL_CORES})
    if(num_cpu_link GREATER num_cpu_memory)
        set(num_cpu_link ${num_cpu_memory})
    endif()
    if(num_cpu_compile GREATER num_cpu_memory)
        set(num_cpu_compile ${num_cpu_memory})
    endif()

    if(CMAKE_SCRIPT_MODE_FILE)
        # The message() command, without any mode, will print to stderr. But jenkins only allows us
        # to capture stdout. To print a clean message without hyphens, we use cmake's echo command.
        execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${num_cpu_compile}")
    else()
        message(STATUS "Parallelism: Available physical memory: ${AVAILABLE_PHYSICAL_MEMORY} / ${TOTAL_PHYSICAL_MEMORY}")
        message(STATUS "Parallelism: Available virtual memory: ${AVAILABLE_VIRTUAL_MEMORY} / ${TOTAL_VIRTUAL_MEMORY}")
        message(STATUS "Parallelism: Number of physical cores: ${NUMBER_OF_PHYSICAL_CORES}")
        message(STATUS "Parallelism: Limiting link pool to ${num_cpu_link}")
        message(STATUS "Parallelism: Limiting compile pool to ${num_cpu_compile}")
    endif()

    # Limit parallelism based on number of physical cores + available memory.
    set_property(GLOBAL PROPERTY JOB_POOLS
        pool-link=${num_cpu_link}
        pool-compile=${num_cpu_compile}
        pool-precompile-header=${num_cpu_compile}
    )
    set(CMAKE_JOB_POOL_LINK              "pool-link"              CACHE STRING "Job pool for linking" FORCE)
    set(CMAKE_JOB_POOL_COMPILE           "pool-compile"           CACHE STRING "Job pool for compiling" FORCE)
    set(CMAKE_JOB_POOL_PRECOMPILE_HEADER "pool-precompile-header" CACHE STRING "Job pool for generating pre-compiled headers" FORCE)

    # Note: We cannot set directly CMAKE_BUILD_PARALLEL_LEVEL or CTEST_PARALLEL_LEVEL from this CMake file,
    # since those are environment variables [1]: they are not cached and do not affect subsequent CMake calls.
    # In practice, the parallelism for Ninja should be limited by our job pools, so the only thing we need is
    # to run ctest in parallel.
    # [1]: https://cmake.org/cmake/help/latest/manual/cmake-language.7.html#cmake-language-environment-variables
endfunction()

# If this file is run in script mode, calling this function will simply echo the total number of
# cores that we desire to build/test with.
if(CMAKE_SCRIPT_MODE_FILE)
    if(DEFINED CMAKE_ARGV3)
        # We need to extract build type from preset
        execute_process(COMMAND ${CMAKE_COMMAND} --preset ${CMAKE_ARGV3} -N OUTPUT_VARIABLE output_log COMMAND_ERROR_IS_FATAL ANY)
        if(output_log MATCHES "CMAKE_BUILD_TYPE=\"([a-zA-Z]+)\"")
            set(CMAKE_BUILD_TYPE ${CMAKE_MATCH_1})
        else()
            message(FATAL_ERROR "Could not find CMAKE_BUILD_TYPE from preset")
        endif()
    endif()
    lagrange_limit_parallelism()
endif()
