#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
function(lagrange_has_onetbb)
    if(NOT TARGET TBB::tbb)
        message(FATAL_ERROR "CMake target TBB::tbb must be defined.")
    endif()

    # This is a very ad-hoc system. Ideally we would use check_cxx_source_compiles() to check the
    # major version macro TBB_INTERFACE_VERSION_MAJOR, but this is a very limited CMake function
    # that does not understand CMake targets not generator expression, making it difficult to test
    # it against the real target TBB::tbb.
    get_target_property(TBB_SOURCES TBB::tbb SOURCES)
    if("arena_slot.cpp" IN_LIST TBB_SOURCES)
        message(STATUS "Lagrange detected OneTBB 2021 or higher")
        set(LAGRANGE_HAS_ONETBB ON CACHE INTERNAL "TBB version used in Lagrange" FORCE)
    else()
        message(STATUS "Lagrange detected TBB 2020 or lower")
        set(LAGRANGE_HAS_ONETBB OFF CACHE INTERNAL "TBB version used in Lagrange" FORCE)
    endif()

    # The code below should be preferred to check the TBB version from the provided target. We keep
    # it commented for reference, but CMake limitations prevent this from being fully functional.
    #
    # include(CMakePushCheckState)
    # include(CheckCXXSourceCompiles)
    # cmake_push_check_state(RESET)
    # check_cxx_source_compiles( [[
    #     #include <tbb/task.h>
    #     #if TBB_INTERFACE_VERSION_MAJOR >= 12
    #         #message("OneTBB 2021 or higher")
    #     #else
    #         #error("TBB 2020 or lower")
    #     #endif
    #     ]]
    #     LAGRANGE_HAS_ONETBB
    # )
    # cmake_pop_check_state()
endfunction()
