#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
function(lagrange_add_python_binding)
    # Retrieve module name
    get_filename_component(module_path "${CMAKE_CURRENT_SOURCE_DIR}/.." REALPATH)
    get_filename_component(module_name "${module_path}" NAME)

    lagrange_include_modules(python)

    # Create interface target for python binding.
    file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
    target_sources(lagrange_python PRIVATE ${SRC_FILES})
    target_link_libraries(lagrange_python PRIVATE lagrange::${module_name})
    target_include_directories(lagrange_python PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    set_property(TARGET lagrange_python APPEND PROPERTY LAGRANGE_ACTIVE_MODULES ${module_name})
endfunction()
