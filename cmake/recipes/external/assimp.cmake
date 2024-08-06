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
if(TARGET assimp::assimp)
    return()
endif()

message(STATUS "Third-party (external): creating target 'assimp::assimp'")

option(BUILD_SHARED_LIBS "Build package with shared libraries." OFF)
option(BUILD_TESTING "" OFF)
option(ASSIMP_BUILD_FRAMEWORK "Build package as Mac OS X Framework bundle." OFF)
option(ASSIMP_DOUBLE_PRECISION "Set to ON to enable double precision processing" OFF)
option(ASSIMP_NO_EXPORT "Disable Assimp's export functionality." OFF)
option(ASSIMP_BUILD_ZLIB "Build your own zlib" ON)
option(ASSIMP_BUILD_ASSIMP_TOOLS "If the supplementary tools for Assimp are built in addition to the library." OFF)
option(ASSIMP_BUILD_SAMPLES "If the official samples are built as well (needs Glut)." OFF)
option(ASSIMP_BUILD_TESTS "If the test suite for Assimp is built in addition to the library." OFF)
option(ASSIMP_INSTALL "Disable this if you want to use assimp as a submodule." OFF)
option(ASSIMP_INSTALL_PBD "" OFF)
option(ASSIMP_INJECT_DEBUG_POSTFIX "Inject debug postfix in .a/.so/.dll lib names" OFF)
option(ASSIMP_BUILD_PBRT_EXPORTER "Build Assimp with PBRT importer" OFF)

# Disable 3MF exporter, since it requires kuba--/zip (which wraps a modified version of miniz)
option(ASSIMP_BUILD_3MF_EXPORTER "Build Assimp with 3MF exporter" OFF)

# Use a CACHE variable to prevent Assimp from building with its embedded clipper
set(Clipper_SRCS "" CACHE STRING "" FORCE)

# Disable IFC importer, since it requires clipper
option(ASSIMP_BUILD_IFC_IMPORTER "Build Assimp with IFC importer" OFF)

include(CPM)
CPMAddPackage(
    NAME assimp
    GITHUB_REPOSITORY assimp/assimp
    GIT_TAG c1deb808faadd85a7a007447b62ae238a4be2337
)

set_target_properties(assimp PROPERTIES FOLDER third_party/assimp)
if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER third_party/assimp)
endif()
set_target_properties(zlibstatic PROPERTIES FOLDER third_party)
