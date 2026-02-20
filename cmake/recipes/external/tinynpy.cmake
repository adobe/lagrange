#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET TinyNPY::TinyNPY)
    return()
endif()

message(STATUS "Third-party (external): creating target 'TinyNPY::TinyNPY'")

include(CPM)
CPMAddPackage(
    NAME tinynpy
    GITHUB_REPOSITORY cdcseacave/TinyNPY
    GIT_TAG v1.1
    DOWNLOAD_ONLY ON
)

add_library(TinyNPY "${tinynpy_SOURCE_DIR}/TinyNPY.cpp" "${tinynpy_SOURCE_DIR}/TinyNPY.h")
add_library(TinyNPY::TinyNPY ALIAS TinyNPY)

include(miniz)
target_link_libraries(TinyNPY PUBLIC miniz::miniz)

# Copy miniz.h as zlib.h to have TinyNPY use miniz symbols (which are aliased through #define in miniz.h)
FetchContent_GetProperties(miniz)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(${miniz_SOURCE_DIR}/miniz.h ${CMAKE_CURRENT_BINARY_DIR}/include/zlib.h COPYONLY)

include(GNUInstallDirs)
target_include_directories(TinyNPY SYSTEM BEFORE PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

set_target_properties(TinyNPY PROPERTIES
    DEFINE_SYMBOL "TINYNPY_EXPORT"
    VERSION "1.1.0"
    SOVERSION "1"
)

target_include_directories(TinyNPY PUBLIC
    "$<BUILD_INTERFACE:${tinynpy_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(TinyNPY
    INTERFACE $<$<BOOL:${BUILD_SHARED_LIBS}>:TINYNPY_IMPORT>
    PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

set(unix_compilers "AppleClang;Clang;GNU")
if(CMAKE_CXX_COMPILER_ID IN_LIST unix_compilers) # IN_LIST wants the second arg to be a var
    target_compile_options(TinyNPY PRIVATE "-frtti")
endif()

set_target_properties(TinyNPY PROPERTIES FOLDER third_party)
