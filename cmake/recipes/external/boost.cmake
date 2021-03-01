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
if(TARGET Boost::boost)
    return()
endif()

message(STATUS "Third-party (external): creating targets 'Boost::boost'...")

include(FetchContent)
FetchContent_Declare(
    boost-cmake
    GIT_REPOSITORY https://github.com/Orphis/boost-cmake.git
    GIT_TAG 70b12f62da331dd402b78102ec8f6a15d59a7af9
)

set(PREVIOUS_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(OLD_CMAKE_POSITION_INDEPENDENT_CODE ${CMAKE_POSITION_INDEPENDENT_CODE})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# This guy will download boost using FetchContent
FetchContent_MakeAvailable(boost-cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ${OLD_CMAKE_POSITION_INDEPENDENT_CODE})
set(CMAKE_CXX_FLAGS "${PREVIOUS_CMAKE_CXX_FLAGS}")
