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

# In order to copy runtime dependencies, we register all executable created by Lagrange into a global property. Then,
# at the end of the CMake script (once all targets have their list of dependencies fully defined), the user must call
# `lagrange_copy_all_runtime_dependencies` to create the actual copy rules for each executable.

function(lagrange_add_executable name)
    # Retrieve options
    set(options WITH_UI)
    set(oneValueArgs "")
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 1 OPTIONS "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Creates a target for executable
    add_executable(${name} ${OPTIONS_UNPARSED_ARGUMENTS})

    # Defines a PRE_LINK custom command to copy target runtime dependencies into target output folder
    include(lagrange_runtime_dependencies)
    lagrange_prepare_runtime_dependencies(${name})

    # Registers the executable name into a global property, so we can list all such executables later on
    get_property(lagrange_executables GLOBAL PROPERTY __lagrange_executables)
    list(APPEND lagrange_executables ${name})
    set_property(GLOBAL PROPERTY __lagrange_executables ${lagrange_executables})

    # Apply Emscripten-specific settings when building for WebAssembly.
    if(EMSCRIPTEN)
        # Use different settings for UI examples and CLI examples.
        if(OPTIONS_WITH_UI)
            # Set executable suffix to ".html".
            set_target_properties(${name} PROPERTIES SUFFIX ".html")

            # Use "-s ENVIRONMENT=web,webview,worker" to restrict the example to the browser.
            # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L616.

            # Use "--emrun" to allow running with the emrun command.
            # See https://emscripten.org/docs/compiling/Running-html-files-with-emrun.html.

            target_link_options(${name} PUBLIC
                "SHELL:-s ENVIRONMENT=web,webview,worker"
                --emrun
            )
        else()
            # Use "-s ENVIRONMENT=node,worker" to restrict the example to Node.js.
            # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L616.

            # Use "-s NODERAWFS=1" to allow access to the system's files (through Emscripten's "raw
            # filesystem" backend).
            # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L898.

            # Use "-s EXIT_RUNTIME=1" to exit the Node.js process when the main thread completes.
            # Otherwise, any worker threads (even completed threads) will keep the process alive.
            # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L91.

            target_link_options(${name} PUBLIC
                "SHELL:-s ENVIRONMENT=node,worker"
                "SHELL:-s NODERAWFS=1"
                "SHELL:-s EXIT_RUNTIME=1"
            )
        endif()
    endif()
endfunction()

function(lagrange_copy_all_runtime_dependencies)
    # Retrieves the list of executables generated by Lagrange
    include(lagrange_runtime_dependencies)
    get_property(lagrange_executables GLOBAL PROPERTY __lagrange_executables)

    # Iterates over Lagrange executable and populate the script containing the target's runtime dependencies
    foreach(target IN ITEMS ${lagrange_executables})
        lagrange_populate_runtime_dependencies(${target})
    endforeach()
endfunction()
