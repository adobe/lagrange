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
if(TARGET opensubdiv::opensubdiv)
    return()
endif()

message(STATUS "Third-party (external): creating target 'opensubdiv::opensubdiv'")

block()
    set(NO_EXAMPLES ON)
    set(NO_TUTORIALS ON)
    set(NO_REGRESSION ON)
    set(NO_PTEX ON)
    set(NO_DOC ON)
    set(NO_OMP ON)
    set(NO_TBB ON)
    set(NO_CUDA ON)
    set(NO_OPENCL ON)
    set(NO_CLEW ON)
    set(NO_OPENGL ON)
    set(NO_METAL ON)
    set(NO_DX ON)
    set(NO_TESTS ON)
    set(NO_GLTESTS ON)
    set(NO_GLEW ON)
    set(NO_GLFW ON)
    set(NO_GLFW_X11 ON)
    set(NO_MACOS_FRAMEWORK ON)

    # We trick OpenSubdiv's CMake into _not_ calling `find_package(TBB)` by setting `TBB_FOUND` to `ON`.
    set(TBB_FOUND ON)
    set(TBB_CXX_FLAGS "")
    include(tbb)

    include(CPM)
    CPMAddPackage(
        NAME opensubdiv
        GITHUB_REPOSITORY PixarAnimationStudios/OpenSubdiv
        GIT_TAG v3_6_0
    )

    # Note: OpenSubdiv doesn't support being compiled as a shared library on Windows:
    # https://github.com/PixarAnimationStudios/OpenSubdiv/issues/71
    if(BUILD_SHARED_LIBS AND TARGET osd_dynamic_cpu)
        add_library(opensubdiv::opensubdiv ALIAS osd_dynamic_cpu)
        set(OPENSUBDIV_TARGET osd_dynamic_cpu)
    else()
        add_library(opensubdiv::opensubdiv ALIAS osd_static_cpu)
        set(OPENSUBDIV_TARGET osd_static_cpu)
    endif()

    # OpenSubdiv's code uses relative header include paths, and fails to properly set a transitive include directory
    # that propagates to dependent targets, so we need to set it up manually.
    include(GNUInstallDirs)
    target_include_directories(${OPENSUBDIV_TARGET} SYSTEM PUBLIC
        $<BUILD_INTERFACE:${opensubdiv_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Set folders for MSVC
    foreach(name IN ITEMS bfr_obj far_obj osd_cpu_obj osd_static_cpu sdc_obj vtr_obj)
        if(TARGET ${name})
            set_target_properties(${name} PROPERTIES FOLDER third_party/opensubdiv/opensubdiv)
        endif()
    endforeach()
    foreach(name IN ITEMS regression_common_obj regression_far_utils_obj)
        if(TARGET ${name})
            set_target_properties(${name} PROPERTIES FOLDER third_party/opensubdiv/regression)
        endif()
    endforeach()
    foreach(name IN ITEMS public_headers)
        if(TARGET ${name})
            set_target_properties(${name} PROPERTIES FOLDER third_party/opensubdiv/public_headers)
        endif()
    endforeach()
endblock()
