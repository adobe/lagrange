#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

####################################################################################################
# This file builds dependency for ImGui+Spectrum, at https://github.com/adobe/imgui
#
# By default, it also builds the glfw + opengl3 example.
# For that to work, it requires glfw and gl3w.
#
# Feel free to change/remove the example and those dependencies.
####################################################################################################

if(TARGET imgui::imgui)
    return()
endif()

include(glfw)
if(NOT EMSCRIPTEN)
    include(gl3w)
endif()

message(STATUS "Third-party (external): creating target 'imgui::imgui' ('docking' branch)")

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/adobe/imgui.git
    GIT_TAG 27dd81adfa98f203e0bbe9ac6682265fee1644c2 # docking_v1.89.1
)
FetchContent_MakeAvailable(imgui)

target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_KEYIO)

set_target_properties(imgui PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(imgui PRIVATE
        "-Wno-ignored-qualifiers" "-Wno-unused-variable"
    )
endif()
