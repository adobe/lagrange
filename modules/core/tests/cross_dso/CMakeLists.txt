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
# Exercise ordinary RTTI across a dynamically loaded MODULE library on native POSIX,
# or a MAIN_MODULE/SIDE_MODULE boundary with Emscripten.
if(UNIX AND NOT EMSCRIPTEN)
    add_library(lagrange_core_cross_dso_test_helper MODULE cross_dso_helper.cpp)
    if(LAGRANGE_TOPLEVEL_PROJECT)
        set_target_properties(lagrange_core_cross_dso_test_helper PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    endif()
    target_link_libraries(lagrange_core_cross_dso_test_helper PRIVATE lagrange::core)
    set_target_properties(lagrange_core_cross_dso_test_helper PROPERTIES
        FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    target_compile_definitions(test_lagrange_core PRIVATE
        LAGRANGE_TEST_CROSS_DSO_HELPER
        LAGRANGE_TEST_CROSS_DSO_HELPER_PATH="$<TARGET_FILE:lagrange_core_cross_dso_test_helper>"
    )
    set_target_properties(test_lagrange_core PROPERTIES ENABLE_EXPORTS ON)
    target_link_libraries(test_lagrange_core PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(test_lagrange_core lagrange_core_cross_dso_test_helper)
elseif(EMSCRIPTEN)
    # Emscripten has no SHARED library type, so the SIDE_MODULE helper is an OBJECT
    # library plus a custom link step (em++ -sSIDE_MODULE=1) that emits helper.wasm.
    add_library(lagrange_core_cross_dso_esm_helper OBJECT cross_dso_helper.cpp)
    if(LAGRANGE_TOPLEVEL_PROJECT)
        set_target_properties(lagrange_core_cross_dso_esm_helper PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    endif()
    target_link_libraries(lagrange_core_cross_dso_esm_helper PRIVATE lagrange::core)
    set_target_properties(lagrange_core_cross_dso_esm_helper PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests"
    )
    # The helper must match the MAIN_MODULE host's shared-memory model:
    # it inherits -pthread when LAGRANGE_USE_WASM_THREADS is on.
    if(LAGRANGE_USE_WASM_THREADS)
        set(_esm_helper_thread_flags "-pthread" "-sSHARED_MEMORY=1" "-sALLOW_MEMORY_GROWTH=1")
    else()
        set(_esm_helper_thread_flags "")
    endif()
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/tests/lagrange_core_cross_dso_esm_helper.wasm"
        COMMAND ${CMAKE_CXX_COMPILER} -sSIDE_MODULE=1 -O1 ${EMSCRIPTEN_EXCEPTION_HANDLER_FLAG} ${_esm_helper_thread_flags} $<TARGET_OBJECTS:lagrange_core_cross_dso_esm_helper> -o "${CMAKE_BINARY_DIR}/tests/lagrange_core_cross_dso_esm_helper.wasm"
        DEPENDS lagrange_core_cross_dso_esm_helper $<TARGET_OBJECTS:lagrange_core_cross_dso_esm_helper>
        COMMENT "Linking Emscripten SIDE_MODULE helper"
    )
    add_custom_target(lagrange_core_cross_dso_esm_helper_wasm DEPENDS "${CMAKE_BINARY_DIR}/tests/lagrange_core_cross_dso_esm_helper.wasm")

    add_executable(test_lagrange_core_cross_dso_esm ../experimental/test_cross_dso_down_cast.cpp)
    if(LAGRANGE_TOPLEVEL_PROJECT)
        set_target_properties(test_lagrange_core_cross_dso_esm PROPERTIES COMPILE_WARNING_AS_ERROR ON)
    endif()
    target_link_libraries(test_lagrange_core_cross_dso_esm PRIVATE
        lagrange::core
        Catch2::Catch2WithMain
    )
    target_link_options(test_lagrange_core_cross_dso_esm PRIVATE
        -sMAIN_MODULE=1
        -ldl
        "SHELL:-s ENVIRONMENT=node,worker"
        "SHELL:-s NODERAWFS=1"
        "SHELL:-s EXIT_RUNTIME=1"
    )
    set_target_properties(test_lagrange_core_cross_dso_esm PROPERTIES
        FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
    target_compile_definitions(test_lagrange_core_cross_dso_esm PRIVATE
        LAGRANGE_TEST_CROSS_DSO_HELPER
        LAGRANGE_TEST_CROSS_DSO_HELPER_PATH="${CMAKE_BINARY_DIR}/tests/lagrange_core_cross_dso_esm_helper.wasm"
    )

    add_test(
        NAME lagrange_core_cross_dso_esm
        COMMAND test_lagrange_core_cross_dso_esm
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
    add_dependencies(test_lagrange_core_cross_dso_esm lagrange_core_cross_dso_esm_helper_wasm)
endif()
