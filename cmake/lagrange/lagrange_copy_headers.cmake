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

# Copy the PUBLIC_HEADER of a given target to a prescribed folder
function(lagrange_copy_headers target output_folder)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "'${target}' is not a CMake target!")
    endif()

    # Note: this doesn't work for INTERFACE target before CMake 3.19:
    # https://gitlab.kitware.com/cmake/cmake/-/issues/19597
    get_target_property(target_dir ${target} SOURCE_DIR)

    get_target_property(public_headers ${target} PUBLIC_HEADER)
    foreach(filename IN ITEMS ${public_headers})
        set(filepath "${target_dir}/${filename}")
        if(NOT EXISTS ${filepath})
            message(FATAL_ERROR "Cannot find source header: ${filepath}")
        endif()
        if(LAGRANGE_VERBOSE)
            message(STATUS "Copying header file ${filename} -> ${output_folder}")
        endif()
        configure_file(${filepath} "${output_folder}/${filename}" COPYONLY)
    endforeach()
endfunction()
