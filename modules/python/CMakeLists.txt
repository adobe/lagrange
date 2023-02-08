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

# 0. Add $loader_path or $ORIGIN to rpath.
if(SKBUILD)
    if(APPLE)
        set(CMAKE_INSTALL_RPATH @loader_path)
    elseif(UNIX)
        set(CMAKE_INSTALL_RPATH $ORIGIN)
    endif()
endif()

# 1. define module
include(nanobind tbb)
nanobind_add_module(lagrange_python NB_STATIC)
add_library(lagrange::python ALIAS lagrange_python)
set_target_properties(lagrange_python PROPERTIES
    FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Modules"
    CXX_VISIBILITY_PRESET default)

if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_python PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

message(STATUS "Lagrange: creating target 'lagrange::python'")
if(TBB_PREFER_STATIC)
    message(FATAL_ERROR "TBB must be compiled as shared library for python binding to work.")
endif()

# 2. installation
if(SKBUILD)
    # Install python extension directly at ${CMAKE_INSTALL_PREFIX}, which will
    # be set by `setup.py`.
    install(TARGETS lagrange_python tbb
        ARCHIVE DESTINATION . COMPONENT Lagrange_Python_Runtime
        LIBRARY DESTINATION . COMPONENT Lagrange_Python_Runtime
        RUNTIME DESTINATION . COMPONENT Lagrange_Python_Runtime
    )
    add_custom_target(lagrange-python-install-runtime
        ${CMAKE_COMMAND}
        -DCMAKE_INSTALL_COMPONENT=Lagrange_Python_Runtime
        -P "${PROJECT_BINARY_DIR}/cmake_install.cmake"
        DEPENDS lagrange_python
    )
endif()

# 3. output location
set_target_properties(lagrange_python PROPERTIES OUTPUT_NAME "lagrange")
set_target_properties(lagrange_python PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 4. generate lagrange_binding.cpp (to be called after all modules are loaded.)
function(lagrange_generate_python_binding_module)
    if (NOT TARGET lagrange_python)
        message(FATAL_ERROR "Target lagrange_python does not exist!")
    endif()

    set(include_lines "")
    set(bind_lines "")
    get_target_property(active_modules lagrange_python LAGRANGE_ACTIVE_MODULES)
    foreach(module_name IN ITEMS ${active_modules})
        list(APPEND include_lines "#include <lagrange/python/${module_name}.h>")
        list(APPEND bind_lines "nb::module_ m_${module_name} = m.def_submodule(\"${module_name}\", \"${module_name} module\")\\\;")
        list(APPEND bind_lines "lagrange::python::populate_${module_name}_module(m_${module_name})\\\;")
    endforeach()

    set(binding_file_in ${PROJECT_BINARY_DIR}/lagrange_binding.cpp.in)
    set(binding_file ${PROJECT_BINARY_DIR}/lagrange_binding.cpp)
    message(STATUS "Generating ${binding_file}")

    file(WRITE ${binding_file_in} [[
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
]])

    foreach(line IN ITEMS ${include_lines})
        file(APPEND ${binding_file_in} ${line}\n)
    endforeach()

    file(APPEND ${binding_file_in} [[
namespace nb = nanobind;
NB_MODULE(lagrange, m)
{
]])
    foreach(line IN ITEMS ${bind_lines})
        file(APPEND ${binding_file_in} ${line}\n)
    endforeach()
    file(APPEND ${binding_file_in} "}")

    configure_file(${binding_file_in} ${binding_file} COPYONLY)
    target_sources(lagrange_python PRIVATE ${binding_file})
endfunction()
