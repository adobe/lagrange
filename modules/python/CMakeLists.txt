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
include(nanobind)
lagrange_find_package(TBB CONFIG REQUIRED)
nanobind_add_module(lagrange_python NB_STATIC)
set_target_properties(nanobind-static PROPERTIES FOLDER third_party)
add_library(lagrange::python ALIAS lagrange_python)
set_target_properties(lagrange_python PROPERTIES
    FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Modules"
    CXX_VISIBILITY_PRESET default)

include(GNUInstallDirs)
target_include_directories(lagrange_python PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(LAGRANGE_TOPLEVEL_PROJECT)
    set_target_properties(lagrange_python PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

if(SKBUILD AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        find_program(HAS_DSYMUTIL dsymutil)
        if(HAS_DSYMUTIL)
            add_custom_command(TARGET lagrange_python POST_BUILD
                COMMAND dsymutil $<TARGET_FILE:lagrange_python>
            )
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_program(HAS_OBJCOPY objcopy)
        if (HAS_OBJCOPY)
            add_custom_command(TARGET lagrange_python POST_BUILD
                COMMAND objcopy --only-keep-debug $<TARGET_FILE:lagrange_python> $<TARGET_FILE:lagrange_python>.debug
                COMMAND objcopy --strip-debug $<TARGET_FILE:lagrange_python>
                COMMAND objcopy --add-gnu-debuglink=$<TARGET_FILE:lagrange_python>.debug $<TARGET_FILE:lagrange_python>
            )
        endif()
    endif()
endif()

message(STATUS "Lagrange: creating target 'lagrange::python'")
if(TBB_PREFER_STATIC)
    message(FATAL_ERROR "TBB must be compiled as shared library for python binding to work.")
endif()

# 2. installation
if(SKBUILD)
    message(STATUS "Lagrange: installing python binding to ${SKBUILD_PLATLIB_DIR}/lagrange")
    install(TARGETS lagrange_python tbb
        DESTINATION ${SKBUILD_PLATLIB_DIR}/lagrange
        COMPONENT Lagrange_Python_Runtime
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if (HAS_DSYMUTIL)
            install(DIRECTORY $<TARGET_FILE:lagrange_python>.dSYM
                DESTINATION ${SKBUILD_PLATLIB_DIR}/lagrange
                COMPONENT Lagrange_Python_Runtime)
        elseif (HAS_OBJCOPY)
            install(FILES $<TARGET_FILE:lagrange_python>.debug
                DESTINATION ${SKBUILD_PLATLIB_DIR}/lagrange
                COMPONENT Lagrange_Python_Runtime)
        endif()
    endif()
endif()

# 3. output location
set_target_properties(lagrange_python PROPERTIES OUTPUT_NAME "lagrange")
set_target_properties(lagrange_python PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 4. generate lagrange_binding.cpp (to be called after all modules are loaded.)
#    and add install rule for stubgen.

function(lagrange_generate_binding_file)
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
#include <lagrange/python/binding.h>
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


function(lagrange_generate_init_file)
    set(init_lines "")
    get_target_property(active_modules lagrange_python LAGRANGE_ACTIVE_MODULES)
    foreach(module_name IN ITEMS ${active_modules})
        list(APPEND init_lines "from .lagrange import ${module_name}")
    endforeach()

    # Generate __init__.py to import all modules.
    set(init_file ${SKBUILD_PLATLIB_DIR}/lagrange/__init__.py)
    file(WRITE ${init_file} [[
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
from .lagrange.core import *
from ._logging import logger
from ._version import __version__

del _logging, lagrange  # type: ignore

# Import all modules.
]])

    foreach(line IN ITEMS ${init_lines})
        file(APPEND ${init_file} ${line}\n)
    endforeach()
endfunction()

function(lagrange_generate_python_binding_module)
    if (NOT TARGET lagrange_python)
        message(FATAL_ERROR "Target lagrange_python does not exist!")
    endif()

    # Generate lagrange_binding.cpp.
    lagrange_generate_binding_file()

    # Generate __init__.py to import all modules.
    if (SKBUILD)
        lagrange_generate_init_file()
    endif()

    # Lastly, add stubgen installation command.
    if(SKBUILD)
        # Create the lagrange directory containing stubs.
        file(MAKE_DIRECTORY ${SKBUILD_PLATLIB_DIR}/lagrange/lagrange)

        # Write __init__.pyi to import all stubs.
        set(init_pyi_file ${SKBUILD_PLATLIB_DIR}/lagrange/lagrange/__init__.pyi)

        # Generate stubs for python binding within the install location.
        get_target_property(active_modules lagrange_python LAGRANGE_ACTIVE_MODULES)
        foreach(module_name IN ITEMS ${active_modules})
            message(STATUS "Generating stub for lagrange.${module_name}")
            file(APPEND ${init_pyi_file} "from . import ${module_name}\n")
            nanobind_add_stub(
                lagrange_python_stubgen_${module_name}
                INSTALL_TIME
                MODULE lagrange.${module_name}
                OUTPUT ${SKBUILD_PLATLIB_DIR}/lagrange/lagrange/${module_name}.pyi
                DEPENDS lagrange_python
                COMPONENT Lagrange_Python_Runtime
                PYTHON_PATH ${SKBUILD_PLATLIB_DIR}/lagrange/
                VERBOSE
            )
        endforeach()
    endif()
endfunction()

# 5. dump lagrange version
if(SKBUILD)
    file(WRITE ${SKBUILD_PLATLIB_DIR}/lagrange/_version.py "__version__='${lagrange_version}'")
endif()
