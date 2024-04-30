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
if(TARGET spdlog::spdlog)
    return()
endif()

message(STATUS "Third-party (external): creating target 'spdlog::spdlog'")

option(SPDLOG_INSTALL "Generate the install target" ON)
option(SPDLOG_FMT_EXTERNAL "Use external fmt library instead of bundled" OFF)

if(SPDLOG_FMT_EXTERNAL)
    include(fmt)
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "spdlog")

# Versions of fmt bundled with spdlog:
# - spdlog 1.13.0 -> fmt 9.1.0
# - spdlog 1.12.0 -> fmt 9.1.0
# - spdlog 1.11.0 -> fmt 9.1.0
# - spdlog 1.10.0 -> fmt 8.1.1
include(CPM)
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG v1.13.0
)

set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(spdlog PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(spdlog PRIVATE
        "-Wno-sign-conversion"
    )
endif()
