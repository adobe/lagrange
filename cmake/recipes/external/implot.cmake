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
if(TARGET implot::implot)
    return()
endif()

message(STATUS "Third-party (external): creating target 'implot::implot'")

include(CPM)
CPMAddPackage(
    NAME implot
    GITHUB_REPOSITORY epezent/implot
    GIT_TAG d65a2bef53d32502407de3a4be80f191e2f412d7 # post-v1.0, includes imgui 1.92.8 AddRect signature fix (#703)
)

add_library(implot STATIC)
target_sources(implot
    PUBLIC
        "${implot_SOURCE_DIR}/implot.h"
    PRIVATE
        "${implot_SOURCE_DIR}/implot_internal.h"
        "${implot_SOURCE_DIR}/implot.cpp"
        "${implot_SOURCE_DIR}/implot_items.cpp"
)
add_library(implot::implot ALIAS implot)

target_include_directories(implot PUBLIC "${implot_SOURCE_DIR}")

include(imgui)
target_link_libraries(implot PUBLIC imgui::imgui)

set_target_properties(implot PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(implot PROPERTIES FOLDER third_party)
