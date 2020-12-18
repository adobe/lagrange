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
if(TARGET filesystem::filesystem)
    return()
endif()

message(STATUS "Third-party (external): creating target 'filesystem::filesystem'")

include(FetchContent)
FetchContent_Declare(
    filesystem
    GIT_REPOSITORY https://github.com/gulrak/filesystem.git
    GIT_TAG        v1.3.2
)
FetchContent_MakeAvailable(filesystem)

target_compile_definitions(ghc_filesystem INTERFACE GHC_WIN_WSTRING_STRING_TYPE)
add_library(filesystem::filesystem ALIAS ghc_filesystem)

# Unfortunately setting GHC_FILESYSTEM_WITH_INSTALL On doesn't work, because
# it gets reset to false by GHC's cmake_dependent_option call. So here is a copy
# of filesystem's install step:
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME filesystem)
install(DIRECTORY ${filesystem_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS ghc_filesystem EXPORT ghcFilesystemConfig)
install(EXPORT ghcFilesystemConfig NAMESPACE ghcFilesystem:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ghcFilesystem)
