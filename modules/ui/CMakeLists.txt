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
option(LAGRANGE_UI_USE_MDL "Use Material Definition Library" OFF)

# 1. define module
add_library(lagrange_ui)
add_library(lagrange::ui ALIAS lagrange_ui)
message(STATUS "Lagrange: creating target 'lagrange::ui'")

if(WIN32)
    target_compile_definitions(lagrange_ui PUBLIC -DNOMINMAX)
endif()

target_compile_features(lagrange_ui PUBLIC cxx_std_17)

include(GNUInstallDirs)
target_include_directories(lagrange_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 2. target sources
file(GLOB_RECURSE INC_FILES "include/*.h")
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
target_sources(lagrange_ui PRIVATE ${INC_FILES} ${SRC_FILES})

# 3. dependencies
lagrange_include_modules(io)
include(gl3w)
include(glfw)
include(stb)
include(imgui)
include(imguizmo)
include(imgui_fonts)
include(nativefiledialog)
include(nlohmann_json)
include(lagrange-assets)
target_link_libraries(imgui PUBLIC Eigen3::Eigen)
target_compile_definitions(imgui PUBLIC
    IMGUI_USER_CONFIG="${PROJECT_SOURCE_DIR}/modules/ui/include/lagrange/ui/imconfig.h"
)
target_link_libraries(lagrange_ui
    PRIVATE
        lagrange::assets
        stb::stb
    PUBLIC
        lagrange::core
        lagrange::io
        gl3w::gl3w
        glfw::glfw
        imgui::imgui
        imguizmo::imguizmo
        imgui::fonts
        nativefiledialog::nativefiledialog
        nlohmann_json::nlohmann_json
)


find_package(OpenGL REQUIRED OPTIONAL_COMPONENTS OpenGL)
if(TARGET OpenGL::OpenGL)
    target_link_libraries(lagrange_ui PUBLIC OpenGL::OpenGL)
else()
    target_link_libraries(lagrange_ui PUBLIC OpenGL::GL)
endif()

if(LAGRANGE_UI_USE_MDL)
    include(mdl)
    target_compile_definitions(lagrange_ui PUBLIC LAGRANGE_UI_USE_MDL)
    target_link_libraries(lagrange_ui PUBLIC mdl::mdl)
endif()

# 4. automatic shader generation
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated_dir}")
target_include_directories(lagrange_ui PUBLIC "${generated_dir}")

add_custom_target(lagrange_ui_generate_shaders
    COMMAND ${CMAKE_COMMAND}
    -DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/"
    -DGENERATED_DIR="${generated_dir}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lagrange_ui_generate_shaders.cmake"
)
set_target_properties(lagrange_ui_generate_shaders PROPERTIES FOLDER "Lagrange")

add_dependencies(lagrange_ui lagrange_ui_generate_shaders)

# PCH
if(LAGRANGE_USE_PCH)
    add_library(lagrange_ui_pch INTERFACE)

    add_library(lagrange::ui_pch ALIAS lagrange_ui_pch)
    target_link_libraries(lagrange_ui_pch INTERFACE lagrange_core_pch)
    target_precompile_headers(lagrange_ui_pch INTERFACE
        <imgui.h>
        <nlohmann/json.hpp>

        <lagrange/ui/Camera.h>
        <lagrange/ui/IBL.h>
        <lagrange/ui/Light.h>
        <lagrange/ui/Material.h>
        <lagrange/ui/MeshBufferFactory.h>
        <lagrange/ui/MeshModel.h>
        <lagrange/ui/MeshModelBase.h>
        <lagrange/ui/ProxyMesh.h>
        <lagrange/ui/Renderer.h>
        <lagrange/ui/RenderUtils.h>
        <lagrange/ui/RenderResourceBuilder.h>
        <lagrange/ui/Scene.h>
        <lagrange/ui/SelectionUI.h>
        <lagrange/ui/Texture.h>
        <lagrange/ui/UIWidget.h>
        <lagrange/ui/Viewer.h>
        <lagrange/ui/Viewport.h>
        <lagrange/ui/ViewportUI.h>
    )
    target_compile_features(lagrange_ui_pch INTERFACE cxx_std_17)

    target_link_libraries(lagrange_ui PRIVATE lagrange::ui_pch lagrange::core_pch)
endif()

# 5. installation
# if(LAGRANGE_INSTALL)
#     set_target_properties(lagrange_ui PROPERTIES EXPORT_NAME ui)
#     lagrange_install(lagrange_ui)
# endif()

# 6. unit tests and examples
if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()
