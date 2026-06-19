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
if(TARGET polyscope::polyscope)
    return()
endif()

message(STATUS "Third-party (external): creating target 'polyscope::polyscope'")

if(NOT TARGET stb)
    include(stb)
    add_library(stb INTERFACE)
    target_link_libraries(stb INTERFACE stb::image stb::image_write)
endif()
include(glfw)
include(imgui)
include(nlohmann_json)
include(glad)
include(implot)
include(imguizmo)

block()
    include(CPM)
    set(BUILD_SHARED_LIBS OFF)
    CPMAddPackage(
        NAME polyscope
        GITHUB_REPOSITORY jdumas/polyscope
        GIT_TAG 3e57795adc4eefae5ec8c4a5afb03ef62e99c110 # jdumas/imgui: imgui 1.92.8 + AddRect fix + ImGuizmo -> nmwsharp@097e4da
    )
endblock()

# polyscope expects imgui::imgui to contains implot and imguizmo.
# explicitly link them here to ensure proper build.
target_link_libraries(polyscope
    PUBLIC
        implot::implot
        imguizmo::imguizmo
)

add_library(polyscope::polyscope ALIAS polyscope)
set_target_properties(polyscope PROPERTIES FOLDER third_party)
set_target_properties(glm PROPERTIES FOLDER third_party)
target_compile_features(polyscope PUBLIC cxx_std_17)
