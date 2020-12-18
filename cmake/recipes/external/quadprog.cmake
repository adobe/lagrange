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
if(TARGET quadprog::quadprog)
    return()
endif()

message(STATUS "Third-party (external): creating target 'quadprog::quadprog'")

include(FetchContent)
FetchContent_Declare(
    quadprog
    GIT_REPOSITORY https://github.com/ggael/QuadProg.git
    GIT_TAG c031c027671488fd13ef3569f9d6319b4e2fec5c
)
FetchContent_MakeAvailable(quadprog)

add_library(quadprog
    ${quadprog_SOURCE_DIR}/QuadProgPP.cpp
    ${quadprog_SOURCE_DIR}/QuadProgPP.h
)
add_library(quadprog::quadprog ALIAS quadprog)
set_target_properties(quadprog PROPERTIES FOLDER third_party)

include(GNUInstallDirs)
target_include_directories(quadprog SYSTEM INTERFACE
    $<BUILD_INTERFACE:${quadprog_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(eigen)
target_link_libraries(quadprog PUBLIC Eigen3::Eigen)

# Install rules
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME quadprog)
install(FILES ${quadprog_SOURCE_DIR}/QuadProgPP.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS quadprog EXPORT Quadprog_Targets)
install(EXPORT Quadprog_Targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quadprog NAMESPACE quadprog::)
