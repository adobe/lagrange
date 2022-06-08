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
add_subdirectory(ui_playground)
add_subdirectory(ui_show_attribute)
add_subdirectory(ui_dynamic_mesh)
add_subdirectory(ui_scene)
add_subdirectory(ui_callbacks)
add_subdirectory(ui_treenode)

include(cli11)

if(NOT TARGET lagrange_download_data)
    lagrange_download_data()
endif()


function(lagrange_ui_add_example name)
    lagrange_add_example(${name} WITH_UI ${name}.cpp)
    target_link_libraries(${name} lagrange::ui CLI11::CLI11)

    # Folder in IDE
    set_target_properties(${name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Examples//UI")

    # Output directory on disk
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/UI")

    # Test data needed for examples
    add_dependencies(${name} lagrange_download_data)

    target_compile_definitions(
        ${name}
        PRIVATE
        LAGRANGE_DATA_FOLDER="${LAGRANGE_DATA_FOLDER}"
        )

    if(EMSCRIPTEN)
        target_link_options(${name} PUBLIC
            "SHELL:-s FORCE_FILESYSTEM"
        )       
    endif()
        

endfunction()

lagrange_ui_add_example(101_Viewer)
lagrange_ui_add_example(102_MeshFromFile)
lagrange_ui_add_example(103_MeshFromMemory)
lagrange_ui_add_example(104_MainLoop)
lagrange_ui_add_example(105_Keybinds)
lagrange_ui_add_example(106_Mouse)
lagrange_ui_add_example(107_UIPanel)
lagrange_ui_add_example(108_FileDialog)
