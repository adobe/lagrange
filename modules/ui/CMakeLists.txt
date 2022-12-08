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
option(LAGRANGE_UI_USE_MDL "Use Material Definition Library" OFF)

# 1. define module
lagrange_add_module()

if(WIN32)
    target_compile_definitions(lagrange_ui PUBLIC -DNOMINMAX -D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING)
endif()

target_compile_features(lagrange_ui PUBLIC cxx_std_17)

# 2. dependencies
lagrange_include_modules(io)
include(nanoflann)
if(NOT EMSCRIPTEN)
    include(gl3w)
endif()
include(glfw)
include(stb)
include(imgui)
include(imguizmo)
include(imgui_fonts)
include(nlohmann_json)
include(lagrange-assets)
include(entt)
include(portable_file_dialogs)
target_link_libraries(imgui PUBLIC Eigen3::Eigen)

get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)

get_filename_component(IMCONFIG_ABS_PATH "${lagrange_source_dir}/modules/ui/include/lagrange/ui/imgui/imconfig.h" ABSOLUTE)

target_compile_definitions(imgui PUBLIC
    IMGUI_USER_CONFIG="${IMCONFIG_ABS_PATH}"
)

target_link_libraries(lagrange_ui
    PRIVATE
        lagrange::assets
        stb::stb
        portable_file_dialogs::portable_file_dialogs
    PUBLIC
        lagrange::core
        lagrange::io
        glfw::glfw
        imgui::imgui
        imguizmo::imguizmo
        fonts::fontawesome6
        fonts::source_sans_pro_regular
        nlohmann_json::nlohmann_json
        EnTT::EnTT
        nanoflann::nanoflann
)

if(NOT EMSCRIPTEN)
    target_link_libraries(lagrange_ui PUBLIC gl3w::gl3w)
endif()

find_package(OpenGL REQUIRED OPTIONAL_COMPONENTS OpenGL)
if(TARGET OpenGL::OpenGL)
    target_link_libraries(lagrange_ui PUBLIC OpenGL::OpenGL)
elseif(EMSCRIPTEN)
    target_link_libraries(lagrange_ui PUBLIC GL)
    target_link_options(lagrange_ui PUBLIC
        "SHELL:-s MIN_WEBGL_VERSION=2"
        "SHELL:-s MAX_WEBGL_VERSION=2"
        "SHELL:-s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1"
        "SHELL:-s GL_SUPPORT_SIMPLE_ENABLE_EXTENSIONS=1"
        "SHELL:-s ASYNCIFY=1"
    )
else()
    target_link_libraries(lagrange_ui PUBLIC OpenGL::GL)
endif()

if(LAGRANGE_UI_USE_MDL)
    include(mdl)
    target_compile_definitions(lagrange_ui PUBLIC LAGRANGE_UI_USE_MDL)
    target_link_libraries(lagrange_ui PUBLIC mdl::mdl)
endif()

if(LAGRANGE_DEBUG_SHADERS)
    target_compile_definitions(lagrange_ui PRIVATE DEFAULT_SHADERS_USE_REAL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/")
endif()

# 3. automatic shader generation
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated_dir}")
target_include_directories(lagrange_ui PUBLIC "${generated_dir}")

add_custom_target(lagrange_ui_generate_shaders
    COMMAND ${CMAKE_COMMAND}
    -DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/"
    -DGENERATED_DIR="${generated_dir}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lagrange_ui_generate_shaders.cmake"
)
set_target_properties(lagrange_ui_generate_shaders PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Utils")

add_dependencies(lagrange_ui lagrange_ui_generate_shaders)

# PCH
if(LAGRANGE_USE_PCH)
    add_library(lagrange_ui_pch INTERFACE)
    target_link_libraries(lagrange_ui_pch INTERFACE lagrange_core_pch)
    target_precompile_headers(lagrange_ui_pch INTERFACE
        <imgui.h>
        <nlohmann/json.hpp>
        <lagrange/ui/UI.h>
    )
    target_compile_features(lagrange_ui_pch INTERFACE cxx_std_17)
    target_link_libraries(lagrange_ui PRIVATE lagrange_ui_pch)
endif()

# 4. installation
# if(LAGRANGE_INSTALL)
#     set_target_properties(lagrange_ui PROPERTIES EXPORT_NAME ui)
#     lagrange_install(lagrange_ui)
# endif()

# 5. unit tests and examples
if(LAGRANGE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()


