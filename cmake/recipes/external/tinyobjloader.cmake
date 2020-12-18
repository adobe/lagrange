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
if(TARGET tinyobjloader::tinyobjloader)
    return()
endif()

message(STATUS "Third-party (external): creating target 'tinyobjloader::tinyobjloader'")

# Tinyobjloader is a big repo for a single header, so we just download the header...
include(FetchContent)
set(TINYOBJLOADER_VERSION "b200acfe63c1ccbd67948eea4de7ad8f769561b2")
set(TINYOBJLOADER_URL "https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/${TINYOBJLOADER_VERSION}/tiny_obj_loader.h")
set(TINYOBJLOADER_FOLDER "${FETCHCONTENT_BASE_DIR}/tinyobjloader/${TINYOBJLOADER_VERSION}")
set(TINYOBJLOADER_FILE "${TINYOBJLOADER_FOLDER}/tiny_obj_loader.h")

if(NOT EXISTS ${TINYOBJLOADER_FILE})
    message(STATUS "Downloading ${TINYOBJLOADER_URL} to ${TINYOBJLOADER_FILE}")
    file(MAKE_DIRECTORY ${TINYOBJLOADER_FOLDER})
    file(
        DOWNLOAD ${TINYOBJLOADER_URL} ${TINYOBJLOADER_FILE}
        TIMEOUT 60
        TLS_VERIFY ON
        EXPECTED_MD5 e2e94c5277ce209fb857c578272df507
    )
endif()

add_library(tinyobjloader::tinyobjloader INTERFACE IMPORTED GLOBAL)
target_compile_definitions(tinyobjloader::tinyobjloader INTERFACE TINYOBJLOADER_USE_DOUBLE)
target_include_directories(tinyobjloader::tinyobjloader SYSTEM INTERFACE ${TINYOBJLOADER_FOLDER})
