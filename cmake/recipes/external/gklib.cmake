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
if(TARGET GKlib::GKlib)
    return()
endif()

message(STATUS "Third-party (external): creating target 'GKlib::GKlib'")

include(FetchContent)
FetchContent_Declare(
    gklib
    GIT_REPOSITORY https://github.com/KarypisLab/GKlib.git
    GIT_TAG        a7f8172703cf6e999dd0710eb279bba513da4fec
)

FetchContent_GetProperties(gklib)
if(NOT gklib_POPULATED)
    FetchContent_Populate(gklib)
endif()

file(GLOB INC_FILES "${gklib_SOURCE_DIR}/*.h" )
file(GLOB SRC_FILES "${gklib_SOURCE_DIR}/*.c" )
if(NOT MSVC)
    list(REMOVE_ITEM SRC_FILES "${gklib_SOURCE_DIR}/gkregex.c")
endif()

add_library(GKlib STATIC ${INC_FILES} ${SRC_FILES})
add_library(GKlib::GKlib ALIAS GKlib)

if(MSVC)
    target_compile_definitions(GKlib PUBLIC USE_GKREGEX)
    target_compile_definitions(GKlib PUBLIC "__thread=__declspec(thread)")
endif()

include(GNUInstallDirs)
target_include_directories(GKlib SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${gklib_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Until [1] is merged, workaround this by making a copying the missing header so it can be found
# during compilation.
# [1]: https://github.com/KarypisLab/GKlib/pull/19
file(MAKE_DIRECTORY "${gklib_BINARY_DIR}/include/gklib")
configure_file("${gklib_SOURCE_DIR}/gk_ms_stdint.h" "${gklib_BINARY_DIR}/include/gklib/ms_stdint.h" COPYONLY)
target_include_directories(GKlib SYSTEM PUBLIC "$<BUILD_INTERFACE:${gklib_BINARY_DIR}/include/gklib>")

set_target_properties(GKlib PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(GKlib PRIVATE
        "-Wno-unused-variable"
        "-Wno-sometimes-uninitialized"
        "-Wno-absolute-value"
        "-Wno-shadow"
    )
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(GKlib PRIVATE
        "-w" # Disallow all warnings from gklib.
    )
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    target_compile_options(GKlib PRIVATE
        "/w" # Disable all warnings from gklib!
    )
endif()

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME gklib)
install(DIRECTORY ${gklib_SOURCE_DIR}/include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS GKlib EXPORT GKlib_Targets)
install(EXPORT GKlib_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gklib NAMESPACE gklib::)
