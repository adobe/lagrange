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
if(TARGET Blosc::blosc)
    return()
endif()

message(STATUS "Third-party (external): creating target 'Blosc::blosc'")

# TODO: Use external zlib (via miniz)
# option(PREFER_EXTERNAL_ZLIB "Find and use external Zlib library instead of included sources." ON)
# include(miniz)

block()
    set(BUILD_TESTS OFF)

    include(CPM)
    CPMAddPackage(
        NAME blosc
        GITHUB_REPOSITORY Blosc/c-blosc
        GIT_TAG v1.21.5
    )
endblock()

set_target_properties(blosc_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET Blosc::blosc)
    add_library(Blosc::blosc ALIAS blosc_static)
endif()

foreach(name IN ITEMS
    blosc_static
    blosc_shared
    bench
    fuzz_compress
    fuzz_decompress
)
    set_target_properties(${name} PROPERTIES FOLDER third_party/blosc)
endforeach()
