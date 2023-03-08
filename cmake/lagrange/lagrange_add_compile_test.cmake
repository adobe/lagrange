#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
function(lagrange_add_compile_test source_filename)
    # Get file basename and compute target name
    get_filename_component(basename ${source_filename} NAME_WE)
    set(target_name lagrange_compile_${basename})

    # Whether to skip compilation tests altogether
    if(NOT LAGRANGE_COMPILE_TESTS)
        message(STATUS "Ignoring compilation test '${target_name}'")
        return()
    else()
        message(STATUS "Registering compilation test '${target_name}'")
    endif()

    # Retrieve module name
    get_filename_component(module_path "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
    get_filename_component(module_name "${module_path}" NAME)

    # Create target library
    add_library(${target_name} STATIC EXCLUDE_FROM_ALL "${source_filename}")

    # Dependencies
    lagrange_include_modules(testing)
    target_link_libraries(${target_name} PUBLIC
        lagrange::${module_name}
        lagrange::testing
    )

    # Set IDE folder
    set_target_properties(${target_name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

    # Register test
    set(test_name compile_${basename})
    add_test(NAME ${test_name} COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ${target_name})
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
endfunction()
