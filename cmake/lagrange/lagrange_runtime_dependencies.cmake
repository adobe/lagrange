#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# Transitively list all link libraries of a target (recursive call)
function(lagrange_get_dependencies_recursive OUTPUT_VARIABLE TARGET)
    get_target_property(_aliased ${TARGET} ALIASED_TARGET)
    if(_aliased)
        set(TARGET ${_aliased})
    endif()

    get_target_property(_IMPORTED ${TARGET} IMPORTED)
    get_target_property(_TYPE ${TARGET} TYPE)
    if(_IMPORTED OR (${_TYPE} STREQUAL "INTERFACE_LIBRARY"))
        get_target_property(TARGET_DEPENDENCIES ${TARGET} INTERFACE_LINK_LIBRARIES)
    else()
        get_target_property(TARGET_DEPENDENCIES ${TARGET} LINK_LIBRARIES)
    endif()

    # MKL-specific list of runtime dependencies
    get_property(RUNTIME_DEPENDENCIES TARGET ${TARGET} PROPERTY mkl_RUNTIME_DEPENDENCIES)
    if(RUNTIME_DEPENDENCIES)
        list(APPEND TARGET_DEPENDENCIES ${RUNTIME_DEPENDENCIES})
    endif()

    set(VISITED_TARGETS ${${OUTPUT_VARIABLE}})
    foreach(DEPENDENCY IN ITEMS ${TARGET_DEPENDENCIES})
        if(TARGET ${DEPENDENCY})
            get_target_property(_aliased ${DEPENDENCY} ALIASED_TARGET)
            if(_aliased)
                set(DEPENDENCY ${_aliased})
            endif()

            if(NOT (DEPENDENCY IN_LIST VISITED_TARGETS))
                list(APPEND VISITED_TARGETS ${DEPENDENCY})
                lagrange_get_dependencies_recursive(VISITED_TARGETS ${DEPENDENCY})
            endif()
        endif()
    endforeach()
    set(${OUTPUT_VARIABLE} ${VISITED_TARGETS} PARENT_SCOPE)
endfunction()

# Transitively list all link libraries of a target
function(lagrange_get_dependencies OUTPUT_VARIABLE TARGET)
    set(DISCOVERED_TARGETS "")
    lagrange_get_dependencies_recursive(DISCOVERED_TARGETS ${TARGET})
    set(${OUTPUT_VARIABLE} ${DISCOVERED_TARGETS} PARENT_SCOPE)
endfunction()

# Creates a POST_BUILD command to be run in order to copy runtime dependencies (.dll) into targets output folder
function(lagrange_prepare_runtime_dependencies target)
    if(NOT WIN32)
        return()
    endif()

    # Create a custom target property to store the script file to rule for this target. At this stage we do not
    # populate this script file yet (we defer until the end when all dependencies have been defined in CMake)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/runtime_deps")
    set_property(TARGET ${target}
        PROPERTY lagrange_COPY_SCRIPT
            ${CMAKE_BINARY_DIR}/runtime_deps/copy_dll_${target}
    )

    # Create a custom command to do the actual copy. This needs to be executed before Catch2's POST_BUILD command,
    # so we define this as a PRE_LINK command for the executable target.
    add_custom_command(
        TARGET ${target}
        PRE_LINK
        COMMAND
            ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/runtime_deps/copy_dll_${target}_$<CONFIG>.cmake"
        COMMENT "Copying dlls for target ${target}"
    )
endfunction()

# For each target given in argument to this function, copy runtime dependencies (.dll) into target output folder
function(lagrange_populate_runtime_dependencies target)
    if(NOT WIN32)
        return()
    endif()

    # Sanity checks
    get_target_property(TYPE ${target} TYPE)
    if(NOT ${TYPE} STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "lagrange_populate_runtime_dependencies() was called on a non-executable target: ${target}")
    endif()
    get_property(COPY_SCRIPT TARGET ${target} PROPERTY lagrange_COPY_SCRIPT)
    if(NOT COPY_SCRIPT)
        message(FATAL_ERROR
            "You need to call lagrange_prepare_runtime_dependencies() on target ${target}"
            " before calling lagrange_populate_runtime_dependencies()")
    endif()

    # Retrieve all target dependencies
    lagrange_get_dependencies(TARGET_DEPENDENCIES ${target})

    # Iterate over dependencies, and create a copy rule for each .dll that we find
    unset(COPY_SCRIPT_CONTENT)
    foreach(DEPENDENCY IN ITEMS ${TARGET_DEPENDENCIES})
        get_target_property(TYPE ${DEPENDENCY} TYPE)
        if(NOT (${TYPE} STREQUAL "SHARED_LIBRARY" OR ${TYPE} STREQUAL "MODULE_LIBRARY"))
            continue()
        endif()

        # Instruction to copy target file if it exists
        string(APPEND COPY_SCRIPT_CONTENT
            "if(EXISTS \"$<TARGET_FILE:${DEPENDENCY}>\")\n    "
                "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "
                "\"$<TARGET_FILE:${DEPENDENCY}>\" "
                "\"$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${DEPENDENCY}>\")\n"
            "endif()\n"
        )
    endforeach()

    # Finally generate one script for each configuration supported by this generator
    if(COPY_SCRIPT_CONTENT)
        message(STATUS "Populating copy rules for target: ${target}")
        file(GENERATE
            OUTPUT ${COPY_SCRIPT}_$<CONFIG>.cmake
            CONTENT ${COPY_SCRIPT_CONTENT}
        )
    endif()
endfunction()
