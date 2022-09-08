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
if(TARGET metis::metis)
    return()
endif()

message(STATUS "Third-party (external): creating target 'metis::metis'")

include(FetchContent)
FetchContent_Declare(
    metis
    GIT_REPOSITORY https://github.com/KarypisLab/METIS.git
    GIT_TAG        94c03a6e2d1860128c2d0675cbbb86ad4f261256
)

FetchContent_GetProperties(metis)
if(NOT metis_POPULATED)
    FetchContent_Populate(metis)
endif()

# Create metis target
file(GLOB INC_FILES "${metis_SOURCE_DIR}/libmetis/*.h" )
file(GLOB SRC_FILES "${metis_SOURCE_DIR}/libmetis/*.c" )

add_library(metis STATIC ${INC_FILES} ${SRC_FILES})
add_library(metis::metis ALIAS metis)

include(gklib)
target_link_libraries(metis PRIVATE GKlib::GKlib)

target_include_directories(metis PRIVATE "${metis_SOURCE_DIR}/libmetis")

include(GNUInstallDirs)
target_include_directories(metis SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${metis_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(metis PUBLIC -DIDXTYPEWIDTH=32)
target_compile_definitions(metis PUBLIC -DREALTYPEWIDTH=32)

set_target_properties(metis PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(metis PRIVATE
        "-Wno-unused-variable"
        "-Wno-sometimes-uninitialized"
        "-Wno-absolute-value"
        "-Wno-shadow"
    )
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(metis PRIVATE
        "-w" # Disallow all warnings from metis.
    )
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    target_compile_options(metis PRIVATE
        "/w" # Disable all warnings from metis!
    )
endif()

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME metis)
install(DIRECTORY ${metis_SOURCE_DIR}/include DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS metis EXPORT Metis_Targets)
install(EXPORT Metis_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/metis NAMESPACE metis::)
