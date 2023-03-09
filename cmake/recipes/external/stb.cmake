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
if(TARGET stb::image)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 5736b15f7ea0ffb08dd38af21067c314d6a3aae9
)
FetchContent_MakeAvailable(stb)

#
# In order to support multiple downstream includes of STB a single macro
# definition needs to be made in a cpp file before including the header.
#

function(stb_make_target header target_name macro_definition)
    set(FULL_TARGET "stb_${target_name}")
    set(TARGET_ALIAS "stb::${target_name}")

    file(WRITE "${stb_BINARY_DIR}/stb_${target_name}.cpp"
         "#define ${macro_definition}\n#include <${header}>\n"
    )

    add_library(
        ${FULL_TARGET} EXCLUDE_FROM_ALL
        ${stb_BINARY_DIR}/stb_${target_name}.cpp
    )
    add_library(${TARGET_ALIAS} ALIAS ${FULL_TARGET})
    set_target_properties(${FULL_TARGET} PROPERTIES FOLDER third_party)
    set_target_properties(${FULL_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(
        ${FULL_TARGET} PUBLIC $<BUILD_INTERFACE:${stb_SOURCE_DIR}>
                              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Install rules
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ${FULL_TARGET})
    install(DIRECTORY ${stb_SOURCE_DIR} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    list(PREPEND STB_TARGETS "${FULL_TARGET}")
    set(STB_TARGETS
        ${STB_TARGETS}
        PARENT_SCOPE
    )
    message(STATUS "Third-party (external): creating target '${TARGET_ALIAS}'")
endfunction()

function(stb_make_target_simple target_name)
    string(TOUPPER "${target_name}" TARGET_NAME_UPPER)
    stb_make_target(
        "stb_${target_name}.h" "${target_name}"
        "STB_${TARGET_NAME_UPPER}_IMPLEMENTATION"
    )
    set(STB_TARGETS
        ${STB_TARGETS}
        PARENT_SCOPE
    )
endfunction()

#
# Make all the stb targets
#
stb_make_target_simple("c_lexer")
# stb_make_target_simple("connected_components") # Requires configuration macro definition
stb_make_target_simple("ds")
# stb_make_target_simple("dxt") # Requires another header
stb_make_target_simple("easy_font")
stb_make_target_simple("herringbone_wang_tile")
stb_make_target_simple("hexwave")
stb_make_target_simple("image_resize")
stb_make_target_simple("image_write")
stb_make_target_simple("image")
stb_make_target_simple("include")
stb_make_target_simple("leakcheck")
stb_make_target_simple("perlin")
stb_make_target_simple("rect_pack")
stb_make_target_simple("sprintf")
# stb_make_target_simple("textedit") # Requires configuration macros definition
# stb_make_target_simple("tilemap_editor") # Requires configuration macro definition
stb_make_target_simple("truetype")
# stb_make_target_simple("voxel_render") # Requires configuration macro definition

#
# Set install for all the stb targets
#
include(GNUInstallDirs)
install(TARGETS ${STB_TARGETS} EXPORT STB_TARGETS)
install(
    EXPORT STB_TARGETS
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stb
    NAMESPACE stb::
)
