#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET piqp::piqp)
    return()
endif()

message(STATUS "Third-party (external): creating target 'piqp::piqp'")

block()
    macro(push_variable var value)
        if(DEFINED CACHE{${var}})
            set(PIQP_OLD_${var}_VALUE "${${var}}")
            set(PIQP_OLD_${var}_TYPE CACHE_TYPE)
        elseif(DEFINED ${var})
            set(PIQP_OLD_${var}_VALUE "${${var}}")
            set(PIQP_OLD_${var}_TYPE NORMAL_TYPE)
        else()
            set(PIQP_OLD_${var}_TYPE NONE_TYPE)
        endif()
        set(${var} "${value}")
    endmacro()

    macro(pop_variable var)
        if(PIQP_OLD_${var}_TYPE STREQUAL CACHE_TYPE)
            set(${var} "${PIQP_OLD_${var}_VALUE}" CACHE PATH "" FORCE)
        elseif(PIQP_OLD_${var}_TYPE STREQUAL NORMAL_TYPE)
            unset(${var} CACHE)
            set(${var} "${PIQP_OLD_${var}_VALUE}")
        elseif(PIQP_OLD_${var}_TYPE STREQUAL NONE_TYPE)
            unset(${var} CACHE)
        else()
            message(FATAL_ERROR "Trying to pop a variable that has not been pushed: ${var}")
        endif()
    endmacro()

    macro(ignore_package NAME VERSION)
        set(PIQP_DUMMY_DIR "${CMAKE_CURRENT_BINARY_DIR}/piqp_cmake/${NAME}")
        file(WRITE ${PIQP_DUMMY_DIR}/${NAME}Config.cmake "")
        include(CMakePackageConfigHelpers)
        write_basic_package_version_file(${PIQP_DUMMY_DIR}/${NAME}ConfigVersion.cmake
            VERSION ${VERSION}
            COMPATIBILITY AnyNewerVersion
            ARCH_INDEPENDENT
        )
        push_variable(${NAME}_DIR ${PIQP_DUMMY_DIR})
        push_variable(${NAME}_ROOT ${PIQP_DUMMY_DIR})
    endmacro()

    macro(unignore_package NAME)
        pop_variable(${NAME}_DIR)
        pop_variable(${NAME}_ROOT)
    endmacro()

    # Prefer Config mode before Module mode to prevent PIQP from loading its own FindEigen3.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    # Import our own targets
    lagrange_find_package(Eigen3 REQUIRED GLOBAL)
    ignore_package(Eigen3 3.4)

    # Ready to include openvdb CMake
    include(CPM)
    CPMAddPackage(
        NAME piqp
        GITHUB_REPOSITORY PREDICT-EPFL/piqp
        GIT_TAG v0.6.0
    )

    unignore_package(Eigen3)

    set_target_properties(piqp PROPERTIES FOLDER third_party)
    set_target_properties(piqp_c PROPERTIES FOLDER third_party)
endblock()
