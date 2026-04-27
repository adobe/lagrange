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
function(lagrange_add_js_binding module_name module_type)
    # module_name: JS key this binding exposes on `lagrange.<module_name>`. Usually matches
    # the folder name; differs for renamed modules (e.g. serialization2 -> serialization).
    # module_type: name of the TypeScript interface exported by ts/${module_name}.ts, used
    # verbatim in the generated registry.ts (e.g. BVHModule, CoreModule, SerializationModule).
    if(NOT module_name)
        message(FATAL_ERROR "lagrange_add_js_binding() requires a JS module name argument")
    endif()
    if(NOT module_type)
        message(FATAL_ERROR "lagrange_add_js_binding(${module_name}) requires a TS interface name")
    endif()

    # C++ library target name: folder name (parent of js/).
    get_filename_component(module_path "${CMAKE_CURRENT_SOURCE_DIR}/.." REALPATH)
    get_filename_component(cpp_target_name "${module_path}" NAME)

    get_property(_initialized GLOBAL PROPERTY LAGRANGE_JS_BINDINGS_INITIALIZED)
    if(NOT _initialized)
        set_property(GLOBAL PROPERTY LAGRANGE_JS_BINDINGS_INITIALIZED TRUE)
        get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)
        get_property(lagrange_binary_dir GLOBAL PROPERTY __lagrange_binary_dir)
        add_subdirectory(
            ${lagrange_source_dir}/bindings/js
            ${lagrange_binary_dir}/bindings/lagrange_js)

        # Clear any stale symlinks from a previous configure (e.g. renamed or
        # removed ts/test files) so they don't get picked up by tsc/vitest.
        # Only removes symlinks — preserves checked-in files like .gitignore.
        file(GLOB _stale_module_links "${lagrange_source_dir}/bindings/js/src/modules/*.ts")
        foreach(_f ${_stale_module_links})
            if(IS_SYMLINK "${_f}")
                file(REMOVE "${_f}")
            endif()
        endforeach()
        file(GLOB _stale_test_links "${lagrange_source_dir}/bindings/js/test/*.ts")
        foreach(_f ${_stale_test_links})
            if(IS_SYMLINK "${_f}")
                file(REMOVE "${_f}")
            endif()
        endforeach()
    endif()

    if(NOT TARGET lagrange_js)
        return()
    endif()

    file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
    target_sources(lagrange_js PRIVATE ${SRC_FILES})
    target_link_libraries(lagrange_js PRIVATE lagrange::${cpp_target_name})
    target_include_directories(lagrange_js PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

    # Symlink TypeScript files from modules/<name>/js/ into bindings/js/
    get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)
    set(js_bindings_dir "${lagrange_source_dir}/bindings/js")

    # Register into the generated registry. lagrange_js_emit_registry emits
    # `bindings/js/src/generated/registry.ts` based on this list. Each registered module must
    # ship a ts/${module_name}.ts file exporting `${module_type}` (interface) and `${module_name}ModuleKeys`.
    set_property(GLOBAL APPEND PROPERTY LAGRANGE_JS_REGISTRY "${module_name}")
    set_property(GLOBAL PROPERTY LAGRANGE_JS_REGISTRY_TYPE_${module_name} "${module_type}")

    # Type files: modules/<name>/js/ts/*.ts → bindings/js/src/modules/*.ts
    set(ts_dir "${CMAKE_CURRENT_SOURCE_DIR}/ts")
    if(EXISTS "${ts_dir}")
        if(NOT EXISTS "${ts_dir}/${module_name}.ts")
            message(FATAL_ERROR
                "lagrange_add_js_binding(${module_name}): expected ${ts_dir}/${module_name}.ts — "
                "TS filename stem must match the JS module name.")
        endif()
        file(GLOB TS_FILES "${ts_dir}/*.ts")
        foreach(ts_file ${TS_FILES})
            get_filename_component(ts_name "${ts_file}" NAME)
            set(link_path "${js_bindings_dir}/src/modules/${ts_name}")
            if(NOT EXISTS "${link_path}")
                file(CREATE_LINK "${ts_file}" "${link_path}" SYMBOLIC)
            endif()
        endforeach()
    endif()

    # Test files: modules/<name>/js/test/*.ts → bindings/js/test/*.ts
    set(test_dir "${CMAKE_CURRENT_SOURCE_DIR}/test")
    if(EXISTS "${test_dir}")
        file(GLOB TEST_FILES "${test_dir}/*.ts")
        foreach(test_file ${TEST_FILES})
            get_filename_component(test_name "${test_file}" NAME)
            set(link_path "${js_bindings_dir}/test/${test_name}")
            if(NOT EXISTS "${link_path}")
                file(CREATE_LINK "${test_file}" "${link_path}" SYMBOLIC)
            endif()
        endforeach()
    endif()
endfunction()
