#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET Blosc::blosc)
    return()
endif()

message(STATUS "Third-party (external): creating target 'Blosc::blosc'")

block()
    macro(push_variable var value)
        if(DEFINED CACHE{${var}})
            set(BLOSC_OLD_${var}_VALUE "${${var}}")
            set(BLOSC_OLD_${var}_TYPE CACHE_TYPE)
        elseif(DEFINED ${var})
            set(BLOSC_OLD_${var}_VALUE "${${var}}")
            set(BLOSC_OLD_${var}_TYPE NORMAL_TYPE)
        else()
            set(BLOSC_OLD_${var}_TYPE NONE_TYPE)
        endif()
        set(${var} "${value}")
    endmacro()

    macro(pop_variable var)
        if(BLOSC_OLD_${var}_TYPE STREQUAL CACHE_TYPE)
            set(${var} "${BLOSC_OLD_${var}_VALUE}" CACHE PATH "" FORCE)
        elseif(BLOSC_OLD_${var}_TYPE STREQUAL NORMAL_TYPE)
            unset(${var} CACHE)
            set(${var} "${BLOSC_OLD_${var}_VALUE}")
        elseif(BLOSC_OLD_${var}_TYPE STREQUAL NONE_TYPE)
            unset(${var} CACHE)
        else()
            message(FATAL_ERROR "Trying to pop a variable that has not been pushed: ${var}")
        endif()
    endmacro()

    macro(ignore_package NAME)
        set(BLOSC_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/blosc_cmake/${NAME}")
        file(WRITE ${BLOSC_DUMMY_DIR}/${NAME}Config.cmake "")
        push_variable(${NAME}_DIR ${BLOSC_DUMMY_DIR})
        push_variable(${NAME}_ROOT ${BLOSC_DUMMY_DIR})
    endmacro()

    macro(unignore_package NAME)
        pop_variable(${NAME}_DIR)
        pop_variable(${NAME}_ROOT)
    endmacro()

    # Prefer Config mode before Module mode to prevent embree from loading its own FindTBB.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    set(BUILD_TESTS OFF)
    set(BUILD_FUZZERS OFF)
    set(PREFER_EXTERNAL_ZLIB ON)
    set(ZLIB_FOUND ON)

    ignore_package(ZLIB)
    include(miniz)
    if(NOT TARGET ZLIB::ZLIB)
        get_target_property(_aliased miniz::miniz ALIASED_TARGET)
        add_library(ZLIB::ZLIB ALIAS ${_aliased})
    endif()
    set(ZLIB_INCLUDE_DIR "")
    set(ZLIB_LIBRARY ZLIB::ZLIB)

    # Copy miniz.h as zlib.h to have blosc use miniz symbols (which are aliased through #define in miniz.h)
    FetchContent_GetProperties(miniz)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
    configure_file(${miniz_SOURCE_DIR}/miniz.h ${CMAKE_CURRENT_BINARY_DIR}/include/zlib.h COPYONLY)

    include(CPM)
    CPMAddPackage(
        NAME blosc
        GITHUB_REPOSITORY Blosc/c-blosc
        GIT_TAG v1.21.5
    )

    include(GNUInstallDirs)
    target_include_directories(blosc_static SYSTEM BEFORE PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
    target_include_directories(blosc_shared SYSTEM BEFORE PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

    unignore_package(ZLIB)
endblock()

set_target_properties(blosc_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET Blosc::blosc)
    add_library(Blosc::blosc ALIAS blosc_static)
endif()

foreach(name IN ITEMS
    blosc_static
    blosc_shared
    bench
)
    set_target_properties(${name} PROPERTIES FOLDER third_party/blosc)
endforeach()
