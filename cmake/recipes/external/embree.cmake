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
if(TARGET embree::embree)
    return()
endif()

message(STATUS "Third-party (external): creating target 'embree::embree'")

include(FetchContent)
FetchContent_Declare(
    embree
    GIT_REPOSITORY https://github.com/embree/embree.git
    GIT_TAG        v3.9.0
    GIT_SHALLOW    TRUE
)

# Set Embree's default options
option(EMBREE_ISPC_SUPPORT "Build Embree with support for ISPC applications." OFF)
option(EMBREE_TUTORIALS    "Enable to build Embree tutorials"                 OFF)
option(EMBREE_STATIC_LIB   "Build Embree as a static library."                ON)
set(EMBREE_TESTING_INTENSITY 0         CACHE STRING "Intensity of testing (0 = no testing, 1 = verify and tutorials, 2 = light testing, 3 = intensive testing.")
set(EMBREE_TASKING_SYSTEM    "TBB"     CACHE STRING "Selects tasking system")
set(EMBREE_MAX_ISA           "DEFAULT" CACHE STRING "Selects highest ISA to support.")

# We want to compile Embree with TBB support, so we need to overwrite Embree's
# `find_package()` and provide variables. The following discussion provide some
# context on how to achieve this:
# - https://gitlab.kitware.com/cmake/cmake/issues/17735
# - https://crascit.com/2018/09/14/do-not-redefine-cmake-commands/
function(embree_import_target)
    macro(ignore_package NAME)
        unset(${NAME}_ROOT CACHE) # Prevent warning CMP0074 due to embree's CMake setting policies based on CMake 3.1.0
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${NAME}/${NAME}Config.cmake "")
        set(${NAME}_DIR ${CMAKE_CURRENT_BINARY_DIR}/${NAME} CACHE PATH "")
    endmacro()

    # Prefer Config mode before Module mode to prevent embree from loading its own FindTBB.cmake
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

    # Note that embree wants to be able to export() its target. If we use `TBB::tbb`
    # directly in `TBB_LIBRARIES` we will get an error. For now we hide tbb's target
    # from the exported dependencies of embree by going through an intermediate
    # IMPORTED library. If the client cares about where all this stuff is installed,
    # they will probably have their own way of installing embree/tbb.
    #
    # See relevant discussion here:
    # - https://gitlab.kitware.com/cmake/cmake/issues/17357
    # - https://gitlab.kitware.com/cmake/cmake/issues/15415
    ignore_package(TBB)

    # We globally link against TBB using old-style folder-based commands (this is bad!), because
    # Embree's CMake is poorly written and does not offer a reliable way to provide a custom TBB
    # target.
    include(tbb)
    set(TBB_INCLUDE_DIRS "")
    set(TBB_LIBRARIES "")
    link_libraries(TBB::tbb)

    # Ready to include embree's atrocious CMake
    FetchContent_MakeAvailable(embree)

    # Disable warnings
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # Embree's subgrid.h is known for causing array subscript out of bound
        # warning.  Embree dev claim the code is correct and it is a GCC bug
        # for misfiring warnings.  See https://github.com/embree/embree/issues/271
        #
        # The issue should be fixed for gcc 9.2.1 and later.
        target_compile_options(embree PRIVATE "-Wno-array-bounds")
    endif()

    # Now we need to do some juggling to propagate the include directory properties
    # along with the `embree` target
    include(GNUInstallDirs)
    target_include_directories(embree SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${embree_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/>"
    )

    add_library(embree::embree ALIAS embree)
endfunction()

# Call via a proper function in order to scope variables such as CMAKE_FIND_PACKAGE_PREFER_CONFIG and TBB_DIR
embree_import_target()

# Some order for IDEs
set_target_properties(embree PROPERTIES FOLDER "third_party//embree")
set_target_properties(algorithms PROPERTIES FOLDER "third_party//embree")
set_target_properties(lexers PROPERTIES FOLDER "third_party//embree")
set_target_properties(math PROPERTIES FOLDER "third_party//embree")
set_target_properties(simd PROPERTIES FOLDER "third_party//embree")
set_target_properties(sys PROPERTIES FOLDER "third_party//embree")
set_target_properties(tasking PROPERTIES FOLDER "third_party//embree")
set_target_properties(uninstall PROPERTIES FOLDER "third_party//embree")
