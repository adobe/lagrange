#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

####################################################################################################
# IMPORTANT
#
# This file defines a single ALIAS target `TBB::tbb`.
#
# Depending on the option TBB_PREFER_STATIC, this alias may point to either the dynamic version
# of TBB, or the static version. The official recommendation is to use the dynamic library:
#
# https://www.threadingbuildingblocks.org/faq/there-version-tbb-provides-statically-linked-libraries
# https://stackoverflow.com/questions/638278/how-to-statically-link-to-tbb
#
# For now we do not have a proper CMake workflow to deal with DLLs, so we default to tbb_static
####################################################################################################

if(TARGET TBB::tbb)
    return()
endif()

message(STATUS "Third-party (external): creating target 'TBB::tbb'")

option(TBB_PREFER_STATIC         "Use the static version of TBB for the alias target" ON)
option(TBB_BUILD_SHARED          "Build TBB shared library" OFF)
option(TBB_BUILD_STATIC          "Build TBB static library" OFF)
option(TBB_BUILD_TBBMALLOC       "Build TBB malloc library" ON)
option(TBB_BUILD_TBBMALLOC_PROXY "Build TBB malloc proxy library" OFF)
option(TBB_BUILD_TESTS           "Build TBB tests and enable testing infrastructure" OFF)
option(TBB_NO_DATE               "Do not save the configure date in the version string" ON)
option(TBB_SET_SOVERSION         "Set the SOVERSION (shared library build version suffix)?" ON)

# Mark those options as advanced so they don't show up in CMake GUI
# Please use TBB_PREFER_STATIC instead
mark_as_advanced(TBB_BUILD_SHARED TBB_BUILD_STATIC)

# Make sure tbb or tbb_static is built, according to the user's option
if(TBB_PREFER_STATIC)
    set(TBB_BUILD_STATIC ON  CACHE BOOL "Build TBB static library" FORCE)
    set(TBB_BUILD_SHARED OFF CACHE BOOL "Build TBB shared library" FORCE)
else()
    set(TBB_BUILD_SHARED ON  CACHE BOOL "Build TBB shared library" FORCE)
    set(TBB_BUILD_STATIC OFF CACHE BOOL "Build TBB static library" FORCE)
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME TBB)

# Using wjakob's fork as it has a better cmake build system
# Change it back to intel's once they fix it
# https://github.com/intel/tbb/issues/6
include(CPM)
CPMAddPackage(
    NAME tbb
    GITHUB_REPOSITORY wjakob/tbb
    GIT_TAG 9e219e24fe223b299783200f217e9d27790a87b0
)

# Install rules for the tbb_static target (not defined by upstream CMakeLists.txt)
if(TBB_INSTALL_TARGETS AND TBB_BUILD_STATIC)
    if(NOT TBB_INSTALL_RUNTIME_DIR)
        set(TBB_INSTALL_RUNTIME_DIR bin)
    endif()
    if(NOT TBB_INSTALL_LIBRARY_DIR)
        set(TBB_INSTALL_LIBRARY_DIR lib)
    endif()
    if(NOT TBB_INSTALL_ARCHIVE_DIR)
        set(TBB_INSTALL_ARCHIVE_DIR lib)
    endif()
    if(NOT TBB_INSTALL_INCLUDE_DIR)
        set(TBB_INSTALL_INCLUDE_DIR include)
    endif()
    if(NOT TBB_CMAKE_PACKAGE_INSTALL_DIR)
        set(TBB_CMAKE_PACKAGE_INSTALL_DIR lib/cmake/tbb)
    endif()
    if(TARGET tbb_interface)
        install(TARGETS tbb_interface EXPORT TBB)
    endif()
    if(TARGET tbb_static)
        install(TARGETS tbb_static EXPORT TBB
                LIBRARY DESTINATION ${TBB_INSTALL_LIBRARY_DIR}
                ARCHIVE DESTINATION ${TBB_INSTALL_ARCHIVE_DIR}
                RUNTIME DESTINATION ${TBB_INSTALL_RUNTIME_DIR})
    endif()
    if(TARGET tbbmalloc_static)
        install(TARGETS tbbmalloc_static EXPORT TBB
                LIBRARY DESTINATION ${TBB_INSTALL_LIBRARY_DIR}
                ARCHIVE DESTINATION ${TBB_INSTALL_ARCHIVE_DIR}
                RUNTIME DESTINATION ${TBB_INSTALL_RUNTIME_DIR})
    endif()
    install(EXPORT TBB DESTINATION ${TBB_CMAKE_PACKAGE_INSTALL_DIR} NAMESPACE TBB:: FILE TBBConfig.cmake)
endif()

# Fix include directories to not explicitly reference the build directory, otherwise install() will complain
function(tbb_fix_include_dirs)
    foreach(name IN ITEMS ${ARGN})
        if(NOT TARGET ${name})
            message(FATAL_ERROR "'${name}' is not a CMake target")
        endif()
        get_target_property(__include_dirs ${name} INTERFACE_INCLUDE_DIRECTORIES)
        set_property(TARGET ${name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            $<BUILD_INTERFACE:${__include_dirs}>
            $<INSTALL_INTERFACE:${TBB_INSTALL_INCLUDE_DIR}>
        )
    endforeach()
endfunction()

# For some reason tbb_tbb's INTERFACE_INCLUDE_DIRECTORIES are empty?!
function(tbb_copy_interface_dirs target)
    foreach(name IN ITEMS ${ARGN})
        if(NOT TARGET ${name})
            message(FATAL_ERROR "'${name}' is not a CMake target")
        endif()
        get_target_property(__include_dirs ${name} INTERFACE_INCLUDE_DIRECTORIES)
        if(__include_dirs)
            set_property(TARGET ${target} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${__include_dirs})
        endif()
    endforeach()
endfunction()

# Generate a dummy .cpp so we can create a "real" target that depends on both tbb and tbbmalloc
# This is needed to trick embree 3.13.0 on Windows, which tries to access the location of the target TBB::tbb
# https://github.com/embree/embree/blob/12b99393438a4cc9e478e33459eed78bec6233fd/common/tasking/CMakeLists.txt#L42
file(WRITE "${tbb_BINARY_DIR}/tbb_dummy.cpp.in" [[
    namespace {
    void dummy() { }
    }
]])

configure_file(${tbb_BINARY_DIR}/tbb_dummy.cpp.in ${tbb_BINARY_DIR}/tbb_dummy.cpp COPYONLY)

# Meta-target that brings both tbb and tbbmalloc.
add_library(tbb_tbb ${tbb_BINARY_DIR}/tbb_dummy.cpp)
add_library(TBB::tbb ALIAS tbb_tbb)
if(TBB_PREFER_STATIC)
    target_link_libraries(tbb_tbb PUBLIC tbb_static tbbmalloc_static)
    tbb_fix_include_dirs(tbb_static)
    tbb_copy_interface_dirs(tbb_tbb tbb_static tbbmalloc_static)
else()
    target_link_libraries(tbb_tbb PUBLIC tbb tbbmalloc)
    tbb_fix_include_dirs(tbb)
    tbb_copy_interface_dirs(tbb_tbb tbb tbbmalloc)
endif()

# Install rules
install(TARGETS tbb_tbb EXPORT TBB)

# Set -fPIC flag and IDE folder name for tbb targets
foreach(name IN ITEMS tbb_def_files tbb_static tbb tbbmalloc tbbmalloc_static tbb_tbb)
    if(TARGET ${name})
        set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set_target_properties(${name} PROPERTIES FOLDER third_party)
    endif()
endforeach()
