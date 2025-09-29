#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET texture_signal_processing::texture_signal_processing)
    return()
endif()

message(STATUS "Third-party (external): creating target 'texture_signal_processing::texture_signal_processing'")

include(CPM)
CPMAddPackage(
    NAME texture_signal_processing
    GITHUB_REPOSITORY mkazhdan/TextureSignalProcessing
    GIT_TAG 77baec2e8b1f40e1c669f048d6805e426869eed6
)

add_library(texture_signal_processing::texture_signal_processing INTERFACE IMPORTED GLOBAL)
target_include_directories(texture_signal_processing::texture_signal_processing
    INTERFACE "${texture_signal_processing_SOURCE_DIR}/include"
)

include(misha)
target_link_libraries(texture_signal_processing::texture_signal_processing INTERFACE misha::misha)
