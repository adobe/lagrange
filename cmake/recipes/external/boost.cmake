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

set(PREVIOUS_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(OLD_CMAKE_POSITION_INDEPENDENT_CODE ${CMAKE_POSITION_INDEPENDENT_CODE})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(BOOST_URL "https://boostorg.jfrog.io/artifactory/main/release/1.71.0/source/boost_1_71_0.tar.bz2" CACHE STRING "Boost download URL")
set(BOOST_URL_SHA256 "d73a8da01e8bf8c7eda40b4c84915071a8c8a0df4a6734537ddde4a8580524ee" CACHE STRING "Boost download URL SHA256 checksum")

include(CPM)
CPMAddPackage(
    NAME boost
    URL ${BOOST_URL}
    URL_HASH SHA256=${BOOST_URL_SHA256}
    DOWNLOAD_ONLY ON
)
set(BOOST_SOURCE ${boost_SOURCE_DIR})
set(Boost_POPULATED ON)

# File lcid.cpp from Boost_locale.cpp doesn't compile on MSVC, so we exclude them from the default
# targets being built by the project (only targets explicitly used by other targets will be built).
CPMAddPackage(
    NAME boost-cmake
    GITHUB_REPOSITORY Orphis/boost-cmake
    GIT_TAG 7f97a08b64bd5d2e53e932ddf80c40544cf45edf
    EXCLUDE_FROM_ALL
)

set(CMAKE_POSITION_INDEPENDENT_CODE ${OLD_CMAKE_POSITION_INDEPENDENT_CODE})
set(CMAKE_CXX_FLAGS "${PREVIOUS_CMAKE_CXX_FLAGS}")

foreach(name IN ITEMS
        atomic
        chrono
        container
        date_time
        filesystem
        iostreams
        log
        system
        thread
        timer
    )
    if(TARGET Boost_${name})
        set_target_properties(Boost_${name} PROPERTIES FOLDER third_party/boost)
    endif()
endforeach()
