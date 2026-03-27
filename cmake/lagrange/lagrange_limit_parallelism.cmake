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
    cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
    cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)

    # Determine build type for memory estimation
    get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_is_multi_config OR NOT DEFINED CMAKE_BUILD_TYPE)
        set(_build_type "debug")
    else()
        string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type)
    endif()

    # Estimated peak RSS per link job (in MB), by build type.
    # Measure with: /usr/bin/time -v cmake --build --preset <preset> -j1 --target lagrange_python
    set(_default_link_memory_release 16000) # 16GB
    set(_default_link_memory_relwithdebinfo 32000) # 32GB
    set(_default_link_memory_debug 32000) # 32GB

    # LTO links can require significantly more memory
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        set(_default_link_memory_release 64000) # 64GB
        set(_default_link_memory_relwithdebinfo 64000) # 64GB
    endif()

    if(DEFINED _default_link_memory_${_build_type})
        set(_link_memory ${_default_link_memory_${_build_type}})
    else()
        set(_link_memory ${_default_link_memory_debug})
    endif()

    math(EXPR num_link_jobs "${TOTAL_PHYSICAL_MEMORY} / ${_link_memory}")
    if(num_link_jobs LESS 1)
        set(num_link_jobs 1)
    endif()

    if(CMAKE_SCRIPT_MODE_FILE)
        # Script mode: echo the number of physical cores for use as the -j flag in Jenkins.
        # Link parallelism is handled separately via Ninja job pools at configure time.
        execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${NUMBER_OF_PHYSICAL_CORES}")
    else()
        message(STATUS "Parallelism: Total physical memory: ${TOTAL_PHYSICAL_MEMORY} MB")
        message(STATUS "Parallelism: Link job memory budget: ${_link_memory} MB (${_build_type})")
        message(STATUS "Parallelism: Limiting link pool to ${num_link_jobs}")

        set_property(GLOBAL PROPERTY JOB_POOLS pool-link=${num_link_jobs})
        set(CMAKE_JOB_POOL_LINK "pool-link" CACHE STRING "Job pool for linking" FORCE)
    endif()
endfunction()

# If this file is run in script mode, it echoes the number of physical cores for use as
# the -j flag for cmake --build and ctest. Link parallelism is not relevant here — it is
# enforced by Ninja job pools set during the configure step.
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
