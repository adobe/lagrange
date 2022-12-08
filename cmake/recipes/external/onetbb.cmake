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
if(TARGET TBB::tbb)
    return()
endif()

message(STATUS "Third-party (external): creating target 'TBB::tbb' (OneTBB)")

# Emscripten sets CMAKE_SYSTEM_PROCESSOR to "x86". Change it to "WASM" to prevent TBB from
# adding machine-specific "-mrtm" and "-mwaitpkg" compile options.
if(EMSCRIPTEN)
    set(CMAKE_SYSTEM_PROCESSOR "WASM")
endif()

include(FetchContent)
FetchContent_Declare(
    tbb
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG 6bfd29bde7e9c47bdfc3ae8a1670ea961c95c78c
)

option(TBB_TEST "Enable testing" OFF)
option(TBB_EXAMPLES "Enable examples" OFF)
option(TBB_STRICT "Treat compiler warnings as errors" OFF)
option(TBB_PREFER_STATIC "Use the static version of TBB for the alias target" ON)
unset(TBB_DIR CACHE)

set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
if(TBB_PREFER_STATIC)
    set(BUILD_SHARED_LIBS OFF CACHE STRING "Build shared library" FORCE)
else()
    set(BUILD_SHARED_LIBS ON CACHE STRING "Build shared library" FORCE)
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tbb)
FetchContent_MakeAvailable(tbb)

set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS} CACHE STRING "Build shared library" FORCE)

if(NOT TARGET TBB::tbb)
    message(FATAL_ERROR "TBB::tbb is still not defined!")
endif()

foreach(name IN ITEMS tbb tbbmalloc tbbmalloc_proxy)
    if(TARGET ${name})
        # Folder name for IDE
        set_target_properties(${name} PROPERTIES FOLDER "third_party//tbb")

        # Force debug postfix for library name. Our pre-compiled MKL library expects "tbb12.dll" (without postfix).
        set_target_properties(${name} PROPERTIES DEBUG_POSTFIX "")

        # Without this macro, TBB will explicitly link against "tbb12_debug.lib" in Debug configs.
        # This is undesirable, since our pre-compiled version of MKL is linked against "tbb12.dll".
        target_compile_definitions(${name} PUBLIC -D__TBB_NO_IMPLICIT_LINKAGE=1)

        # Disable some features and avoid processor-specific code paths when compiling with
        # Emscripten for WebAssembly.
        if(EMSCRIPTEN)
            target_compile_definitions(${name} PRIVATE
                ITT_ARCH=-1
                __TBB_RESUMABLE_TASKS_USE_THREADS=1
                __TBB_DYNAMIC_LOAD_ENABLED=0
                __TBB_WEAK_SYMBOLS_PRESENT=0
            )
        endif()
    endif()
endforeach()
