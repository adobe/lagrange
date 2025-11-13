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

# This is a standalone root CMakeLists.txt to test compilation of our fmt::formatter for Eigen
# types. By (manually) tuning various compile options you should be able to easily try out the
# following configurations:
#
# - Spdlog with internal fmt version
# - External fmt 8, 9, 10, 10.2
# - std::format (C++20)
# - User-provided fmt::format<>
#
cmake_minimum_required(VERSION 3.25)
project(test_fmt)

set(LAGRANGE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

message("LAGRANGE_ROOT: ${LAGRANGE_ROOT}")

list(PREPEND CMAKE_MODULE_PATH "${LAGRANGE_ROOT}/cmake/recipes/external")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPDLOG_INSTALL "Generate the install target" OFF)
option(SPDLOG_FMT_EXTERNAL "Use external fmt library instead of bundled" ON)

lagrange_find_package(spdlog REQUIRED)
lagrange_find_package(Eigen3 REQUIRED GLOBAL)

include(CTest)

include(catch2)
FetchContent_GetProperties(catch2)
include("${catch2_SOURCE_DIR}/extras/Catch.cmake")

add_executable(test_fmt test_fmt.cpp)
target_link_libraries(test_fmt PRIVATE spdlog::spdlog Eigen3::Eigen Catch2::Catch2WithMain)
target_include_directories(test_fmt PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")

# Uncomment to test user-provided formatter
# target_compile_definitions(test_fmt PRIVATE
#     LA_FMT_EIGEN_FORMATTER="${CMAKE_CURRENT_LIST_DIR}/user_fmt_formatter.h"
# )

catch_discover_tests(test_fmt)
