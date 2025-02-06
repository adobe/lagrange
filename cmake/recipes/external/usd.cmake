#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET usd::usd)
    return()
endif()

message(STATUS "Third-party (external): creating target 'usd::usd'")

# Note: the linked USD repo below does NOT work, we need to make our own mirror or push changes to github.
# 1.In all of USD's cmake files, replace:
#   ${CMAKE_SOURCE_DIR} with ${PROJECT_SOURCE_DIR}
#   ${CMAKE_BINARY_DIR} with ${PROJECT_BINARY_DIR}
# 2. comment out `include(Packages)` in the main CMakeLists.txt (line 23).
# 3. usd sets all binary output to build/lib folder. This casues other issues.

option(PXR_BUILD_EXAMPLES "Build examples" OFF)
option(PXR_BUILD_IMAGING "Build imaging components" OFF)
option(PXR_BUILD_TESTS "Build tests" OFF)
option(PXR_BUILD_TUTORIALS "Build tutorials" OFF)
option(PXR_BUILD_USD_TOOLS "Build commandline tools" OFF)
option(PXR_BUILD_USDVIEW "Build usdview" OFF)
option(PXR_ENABLE_GL_SUPPORT "Enable OpenGL based components" OFF)
option(PXR_ENABLE_HDF5_SUPPORT "Enable HDF5 backend in the Alembic plugin for USD" OFF)
option(PXR_ENABLE_METAL_SUPPORT "Enable Metal based components" OFF)
option(PXR_ENABLE_PTEX_SUPPORT "Enable Ptex support" OFF)
option(PXR_ENABLE_PYTHON_SUPPORT "Enable Python based components for USD" OFF)
option(PXR_ENABLE_VULKAN_SUPPORT "Enable Vulkan based components" OFF)
option(PXR_ENABLE_PRECOMPILED_HEADERS "Enable precompiled headers." OFF)

# We want to compile USD with TBB support, so we need to overwrite USD's
# `find_package()` and provide variables. The following discussion provide some
# context on how to achieve this:
# - https://gitlab.kitware.com/cmake/cmake/issues/17735
# - https://crascit.com/2018/09/14/do-not-redefine-cmake-commands/
function(usd_import_target)
    macro(push_variable var value)
        if(DEFINED CACHE{${var}})
            set(USD_OLD_${var}_VALUE "${${var}}")
            set(USD_OLD_${var}_TYPE CACHE_TYPE)
        elseif(DEFINED ${var})
            set(USD_OLD_${var}_VALUE "${${var}}")
            set(USD_OLD_${var}_TYPE NORMAL_TYPE)
        else()
            set(USD_OLD_${var}_TYPE NONE_TYPE)
        endif()
        set(${var} "${value}")
    endmacro()

    macro(pop_variable var)
        if(USD_OLD_${var}_TYPE STREQUAL CACHE_TYPE)
            set(${var} "${USD_OLD_${var}_VALUE}" CACHE PATH "" FORCE)
        elseif(USD_OLD_${var}_TYPE STREQUAL NORMAL_TYPE)
            unset(${var} CACHE)
            set(${var} "${USD_OLD_${var}_VALUE}")
        elseif(USD_OLD_${var}_TYPE STREQUAL NONE_TYPE)
            unset(${var} CACHE)
        else()
            message(FATAL_ERROR "Trying to pop a variable that has not been pushed: ${var}")
        endif()
    endmacro()

    macro(ignore_package NAME)
        set(USD_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/usd_cmake/${NAME}")
        file(WRITE ${USD_DUMMY_DIR}/${NAME}Config.cmake "")
        push_variable(${NAME}_DIR ${USD_DUMMY_DIR})
        push_variable(${NAME}_ROOT ${USD_DUMMY_DIR})
    endmacro()

    macro(unignore_package NAME)
        pop_variable(${NAME}_DIR)
        pop_variable(${NAME}_ROOT)
    endmacro()

    # Prefer Config mode before Module mode to prevent embree from loading its own FindTBB.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    # Import our own targets
    include(tbb)
    ignore_package(TBB)
    set(Tbb_VERSION 2019.0 CACHE STRING "" FORCE)

    # We have to spoof the following variables:
    # - TBB_tbb_LIBRARY
    # - TBB_INCLUDE_DIRS
    set(TBB_tbb_LIBRARY TBB::tbb)
    set(TBB_INCLUDE_DIRS "")

    # Always build USD as shared libraries to allow for plugins. Calling functions such as
    # pxr::UsdStage::IsSupportedFile will call Sdf_GetExtension in fileFormat.cpp, which triggers
    # the search for USD plugins.
    push_variable(BUILD_SHARED_LIBS ON)
    push_variable(CMAKE_DEBUG_POSTFIX "")
    include(CPM)
    CPMAddPackage(
        NAME usd
        GIT_REPOSITORY git@git.corp.adobe.com:thirdparty/USD.git
        GIT_TAG c70ef8cd5da03bb2345746c802ef15b1de4f3814 # lagrange/v24.11
    )
    pop_variable(CMAKE_DEBUG_POSTFIX)
    pop_variable(BUILD_SHARED_LIBS)

    unignore_package(TBB)
endfunction()

# Call via a proper function in order to scope variables such as CMAKE_FIND_PACKAGE_PREFER_CONFIG and TBB_DIR
usd_import_target()

# List potential USD targets
set(USD_base_TARGETS arch tf gf js trace work plug vt)
set(USD_usd_TARGETS ar kind sdf ndr sdr pcp usd usdGeom
    usdVol usdMedia usdShade usdLux usdProc usdRender
    usdHydra usdRi usdSkel usdUI usdUtils usdPhysics)
set(USD_headers_TARGETS arch tf gf js trace work plug
    vt ar kind sdf ndr sdr pcp usd usdGeom usdVol usdMedia
    usdShade usdLux usdProc usdRender usdHydra usdRi usdSkel
    usdUI usdUtils usdPhysics)

# Set IDE folder for targets + add usd:: alias
foreach(name IN ITEMS ${USD_headers_TARGETS})
    if(TARGET ${name}_headerfiles)
        set_target_properties(${name}_headerfiles PROPERTIES FOLDER third_party/pxr/headerfiles)
    endif()
endforeach()

foreach(component IN ITEMS base usd)
    foreach(name IN ITEMS ${USD_${component}_TARGETS})
        if(TARGET ${name})
            set_target_properties(${name} PROPERTIES FOLDER third_party/pxr/${component})
            add_library(usd::${name} ALIAS ${name})
            target_include_directories(${name} INTERFACE
                $<BUILD_INTERFACE:${usd_SOURCE_DIR}>
                $<BUILD_INTERFACE:${usd_BINARY_DIR}/include>
            )
        endif()
    endforeach()
endforeach()

if(TARGET shared_libs)
    set_target_properties(shared_libs PROPERTIES FOLDER third_party/pxr)
endif()
