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
    list(FILTER SRC_FILES EXCLUDE REGEX "compile_errors/.*.cpp$")
    include(lagrange_add_executable)
    set(test_target "test_lagrange_${module_name}")
    lagrange_add_executable(${test_target} ${SRC_FILES} ${OPTIONS_UNPARSED_ARGUMENTS})
    set_target_properties(${test_target} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Tests")

    # Dependencies
    lagrange_include_modules(testing)
    target_link_libraries(${test_target} PUBLIC
        lagrange::${module_name}
        lagrange::testing
    )

    # Use Catch2's provided main() by default
    if(NOT OPTIONS_CUSTOM_MAIN)
        target_link_libraries(${test_target} PUBLIC lagrange::testing::main)
    endif()

    # Enable code coverage
    include(FetchContent)
    target_code_coverage(${test_target} AUTO ALL EXCLUDE "${FETCHCONTENT_BASE_DIR}/*")

    # Sanitizer suppression files to be passed to catch_discover_tests
    set(LAGRANGE_TESTS_ENVIRONMENT
        "TSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/.github/tsan.suppressions.ini"
        "LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/.github/lsan.suppressions.ini"
        "ASAN_SAVE_DUMPS=${module_name}.dmp"
    )

    # Output directory
    set_target_properties(${test_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Register tests
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")

    # When cross-compiling with Emscripten, test discovery at build time can produce truncated JSON
    # output due to stdout flushing issues with PROXY_TO_PTHREAD. Use PRE_TEST to defer discovery to
    # ctest runtime instead. See:
    # - https://github.com/emscripten-core/emscripten/issues/15186
    # - https://github.com/emscripten-core/emscripten/issues/20059
    if(EMSCRIPTEN)
        set(_discovery_mode PRE_TEST)
    else()
        set(_discovery_mode POST_BUILD)
    endif()

    # On Linux, ThreadSanitizer can fail with "FATAL: ThreadSanitizer: unexpected memory mapping" on
    # kernels with high ASLR entropy (e.g. kernel 6.x with vm.mmap_rnd_bits > 28). LLVM 18+ / GCC 15+
    # include an auto-retry that re-executes the process with ASLR disabled
    # (https://github.com/llvm/llvm-project/pull/78351). For older compilers, we work around this by
    # wrapping test discovery and execution with `setarch --addr-no-randomize`.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND USE_SANITIZER MATCHES "([Tt]hread)")
        set(_tsan_needs_aslr_workaround FALSE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
            set(_tsan_needs_aslr_workaround TRUE)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18")
            set(_tsan_needs_aslr_workaround TRUE)
        endif()
        if(_tsan_needs_aslr_workaround)
            find_program(_setarch setarch)
            if(_setarch)
                set_target_properties(${test_target} PROPERTIES
                    CROSSCOMPILING_EMULATOR "${_setarch};${CMAKE_HOST_SYSTEM_PROCESSOR};--addr-no-randomize"
                )
                set(_discovery_mode PRE_TEST)
            else()
                message(WARNING "setarch not found — TSan tests may fail with 'unexpected memory mapping' on high-ASLR kernels")
            endif()
        endif()
    endif()

    if(LAGRANGE_TOPLEVEL_PROJECT AND NOT USE_SANITIZER MATCHES "([Tt]hread)")
        catch_discover_tests(${test_target}
            REPORTER junit
            OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
            OUTPUT_SUFFIX ".xml"
            DISCOVERY_MODE ${_discovery_mode}
            PROPERTIES ENVIRONMENT ${LAGRANGE_TESTS_ENVIRONMENT}
        )
    else()
        catch_discover_tests(${test_target}
            DISCOVERY_MODE ${_discovery_mode}
            PROPERTIES ENVIRONMENT ${LAGRANGE_TESTS_ENVIRONMENT}
        )
    endif()

endfunction()
