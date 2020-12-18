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

message(STATUS "Third-party (external): creating target 'imgui::imgui' ('docking' branch)")

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/adobe/imgui.git
    GIT_TAG 220809e59b8878297b91452b5e67301cd3ca39c5
)
FetchContent_MakeAvailable(imgui)

add_library(imgui
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui_internal.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_spectrum.h"
    "${imgui_SOURCE_DIR}/imgui_spectrum.cpp"
    "${imgui_SOURCE_DIR}/examples/imgui_impl_opengl3.h"
    "${imgui_SOURCE_DIR}/examples/imgui_impl_opengl3.cpp"
    "${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.h"
    "${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
)
add_library(imgui::imgui ALIAS imgui)


# Dark theme option
option(IMGUI_USE_DARK_THEME "Use Dark ImGui Spectrum Theme" OFF)
if(IMGUI_USE_DARK_THEME)
    target_compile_definitions(imgui PUBLIC SPECTRUM_USE_DARK_THEME)
endif()

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GL3W=1
    
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS # to check for obsolete functions

    # IMGUI_USER_CONFIG="${PROJECT_SOURCE_DIR}/path/to/imconfig.h" # to use your own imconfig.h
)


target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")

include(glfw)
include(gl3w)
target_link_libraries(imgui PUBLIC glfw::glfw gl3w::gl3w)

set_target_properties(imgui PROPERTIES FOLDER third_party)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(imgui PRIVATE
        "-Wno-ignored-qualifiers" "-Wno-unused-variable"
    )
endif()
