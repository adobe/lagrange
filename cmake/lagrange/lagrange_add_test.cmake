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
function(lagrange_add_test)
    # Retrieve module name
    get_filename_component(module_path "${CMAKE_CURRENT_SOURCE_DIR}/.." REALPATH)
    get_filename_component(module_name "${module_path}" NAME)

    # Create test executable
    file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
    include(lagrange_add_executable)
    lagrange_add_executable(test_${module_name} ${SRC_FILES})
    set_target_properties(test_${module_name} PROPERTIES FOLDER "Lagrange//Tests")

    # Dependencies
    lagrange_include_modules(testing)
    target_link_libraries(test_${module_name} PUBLIC
        lagrange::${module_name}
        lagrange::testing
    )

    # Output directory
    set_target_properties(test_${module_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Register tests
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")
    catch_discover_tests(test_${module_name}
        REPORTER junit
        OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
        OUTPUT_SUFFIX ".xml"
    )
endfunction()
