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
    lagrange_add_executable(${name} ${ARGN})

    # Folder in IDE
    set_target_properties(${name} PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange//Examples")

    # Output directory on disk
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endfunction()
