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
function(lagrange_add_example name)
    # Retrieve options
    set(options WITH_UI)
    set(oneValueArgs "")
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 1 OPTIONS "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Add executable
    include(lagrange_add_executable)
    lagrange_add_executable(${name} ${OPTIONS_UNPARSED_ARGUMENTS})

    # Folder in IDE
    set_target_properties(${name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Examples")

    # Output directory on disk
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

    # Apply Emscripten-specific settings when building for WebAssembly.
    if(EMSCRIPTEN)
        # Use "-s ALLOW_MEMORY_GROWTH=1" to allow the WASM heap to grow.
        # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L194.

        # Use "-Wno-pthreads-mem-growth" to silence the warning "USE_PTHREADS + ALLOW_MEMORY_GROWTH
        # may run non-wasm code slowly, see https://github.com/WebAssembly/design/issues/1271".
        # Examples don't run much (if any) non-wasm code.

        target_link_options(${name} PUBLIC
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            -Wno-pthreads-mem-growth
        )

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

            # Use "-s PROXY_TO_PTHREAD=1" to move program execution to a worker thread, leaving the
            # main thread available to respond to requests for more worker threads. Without this
            # flag, we'd need to prepopulate a thread pool with enough threads for every example app
            # (with something like "-s PTHREAD_POOL_SIZE=40"), otherwise the main thread would block
            # as soon another thread is launched.
            # See https://emscripten.org/docs/porting/pthreads.html#additional-flags and
            # https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L1019.

            # Use "-s EXIT_RUNTIME=1" to exit the Node.js process when the main thread completes.
            # Otherwise, any worker threads (even completed threads) will keep the process alive.
            # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js#L91.

            target_link_options(${name} PUBLIC
                "SHELL:-s ENVIRONMENT=node,worker"
                "SHELL:-s NODERAWFS=1"
                "SHELL:-s PROXY_TO_PTHREAD=1"
                "SHELL:-s EXIT_RUNTIME=1"
            )
        endif()
    endif()
endfunction()
