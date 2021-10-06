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
if(TARGET IlmBase::Half)
    return()
endif()

# Note: This file will be removed once Half is integrated into OpenVDB
# https://github.com/AcademySoftwareFoundation/openvdb/pull/927

message(STATUS "Third-party (external): creating target 'IlmBase::Half'")

include(FetchContent)
FetchContent_Declare(
    ilmbase
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
    GIT_TAG v2.5.4
)
FetchContent_GetProperties(ilmbase)
if(ilmbase_POPULATED)
    return()
endif()
FetchContent_Populate(ilmbase)

file(GLOB SRC_FILES "${ilmbase_SOURCE_DIR}/IlmBase/Half/*.cpp")
file(GLOB INC_FILES "${ilmbase_SOURCE_DIR}/IlmBase/Half/*.h")
add_library(IlmBase_Half ${SRC_FILES} ${INC_FILES})
add_library(IlmBase::Half ALIAS IlmBase_Half)

# Copy headers into an OpenEXR/ subfolder
file(MAKE_DIRECTORY "${ilmbase_BINARY_DIR}/include/OpenEXR")
foreach(filepath IN ITEMS ${INC_FILES})
    get_filename_component(filename ${filepath} NAME)
    configure_file(${filepath} "${ilmbase_BINARY_DIR}/include/OpenEXR/${filename}" COPYONLY)
endforeach()

include(GNUInstallDirs)
target_include_directories(IlmBase_Half PUBLIC
    $<BUILD_INTERFACE:${ilmbase_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(IlmBase_Half PUBLIC cxx_std_14)

set_target_properties(IlmBase_Half PROPERTIES FOLDER third_party)
