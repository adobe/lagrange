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
if(TARGET rectangle_bin_pack::rectangle_bin_pack)
    return()
endif()

message(STATUS "Third-party (external): creating target 'rectangle_bin_pack::rectangle_bin_pack'")

include(CPM)
CPMAddPackage(
    NAME rectanglebinpack
    GIT_REPOSITORY https://github.com/juj/RectangleBinPack
    GIT_TAG 83e7e1132d93777e3732dfaae26b0f3703be2036
    DOWNLOAD_ONLY ON
)

file(GLOB INC_FILES "${rectanglebinpack_SOURCE_DIR}/*.h")
file(GLOB SRC_FILES "${rectanglebinpack_SOURCE_DIR}/*.cpp")
list(FILTER SRC_FILES EXCLUDE REGEX ".*boost_binpacker\\.cpp$")

add_library(rectangle_bin_pack STATIC ${INC_FILES} ${SRC_FILES})
add_library(rectangle_bin_pack::rectangle_bin_pack ALIAS rectangle_bin_pack)
set_target_properties(rectangle_bin_pack PROPERTIES FOLDER "third_party")
set_target_properties(rectangle_bin_pack PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Copy headers into an include/RectangleBinPack subfolder
file(MAKE_DIRECTORY "${rectanglebinpack_BINARY_DIR}/include/RectangleBinPack")
foreach(filepath IN ITEMS ${INC_FILES})
    get_filename_component(filename ${filepath} NAME)
    configure_file(${filepath} "${rectanglebinpack_BINARY_DIR}/include/RectangleBinPack/${filename}" COPYONLY)
endforeach()

include(GNUInstallDirs)
target_include_directories(rectangle_bin_pack PUBLIC
    $<BUILD_INTERFACE:${rectanglebinpack_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(rectangle_bin_pack INTERFACE RECTANGLE_BIN_PACK_OSS)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME RectangleBinPack)
install(DIRECTORY ${rectanglebinpack_BINARY_DIR}/include/RectangleBinPack DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS rectangle_bin_pack EXPORT RectangleBinPack_Targets)
install(EXPORT RectangleBinPack_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rectangle_bin_pack NAMESPACE rectangle_bin_pack::)
