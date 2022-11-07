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

    # Retrieve options
    set(options CUSTOM_MAIN)
    set(oneValueArgs "")
    set(multiValueArgs "")
    cmake_parse_arguments(OPTIONS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Create test executable
    file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
    include(lagrange_add_executable)
    set(test_target "test_lagrange_${module_name}")
    lagrange_add_executable(${test_target} ${SRC_FILES})
    set_target_properties(${test_target} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

    # Dependencies
    lagrange_include_modules(testing)
    target_link_libraries(${test_target} PUBLIC
        lagrange::${module_name}
        lagrange::testing
    )

    # Use Catch2's provided main() by default
    if(NOT OPTIONS_CUSTOM_MAIN)
        target_link_libraries(${test_target} PUBLIC Catch2::Catch2WithMain)
    endif()

    # Enable code coverage
    include(FetchContent)
    target_code_coverage(${test_target} AUTO ALL EXCLUDE "${FETCHCONTENT_BASE_DIR}/*")

    # TSan suppression file to be passed to catch_discover_tests
    set(LAGRANGE_TESTS_ENVIRONMENT
        # "TSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/scripts/tsan.suppressions"
    )

    # Output directory
    set_target_properties(${test_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Register tests
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")

    # if(LAGRANGE_TOPLEVEL_PROJECT AND NOT EMSCRIPTEN)
    #     catch_discover_tests(${test_target}
    #         REPORTER junit
    #         OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
    #         OUTPUT_SUFFIX ".xml"
    #         PROPERTIES ENVIRONMENT ${LAGRANGE_TESTS_ENVIRONMENT}
    #     )
    # else()
        catch_discover_tests(${test_target}
            PROPERTIES ENVIRONMENT ${LAGRANGE_TESTS_ENVIRONMENT}
        )
    # endif()

endfunction()
