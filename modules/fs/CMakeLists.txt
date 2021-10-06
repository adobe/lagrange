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

# 1. define module
lagrange_add_module()

# 2. dependencies
lagrange_include_modules(core)
target_link_libraries(lagrange_fs PUBLIC
    lagrange::core
)

set(LAGRANGE_FS_BACKEND "ghc"
    CACHE STRING
    "Which file system backend to use? Options are 'std', 'ghc' and 'boost'.  Default is 'ghc'"
)
set(LAGRANGE_FS_BACKEND_CHOICES std ghc boost)
set_property(CACHE LAGRANGE_FS_BACKEND PROPERTY STRINGS ${LAGRANGE_FS_BACKEND_CHOICES})

string(TOLOWER ${LAGRANGE_FS_BACKEND} LAGRANGE_FS_BACKEND)
if (LAGRANGE_FS_BACKEND STREQUAL "ghc")
    include(filesystem)
    target_link_libraries(lagrange_fs PUBLIC filesystem::filesystem)
    target_compile_definitions(lagrange_fs PUBLIC LAGRANGE_USE_GHC_FS)
elseif (LAGRANGE_FS_BACKEND STREQUAL "std")
    target_compile_features(lagrange_fs PUBLIC cxx_std_17)
    target_compile_definitions(lagrange_fs PUBLIC LAGRANGE_USE_STD_FS)
elseif (LAGRANGE_FS_BACKEND STREQUAL "boost")
    # Since boost is not an official dependencies of Lagrange.  We assume
    # Boost::filesystem target exists.  If not, uncomment the following lines:
    # find_package(Boost REQUIRED filesystem)

    target_compile_definitions(lagrange_fs PUBLIC LAGRANGE_USE_BOOST_FS)
    target_link_libraries(lagrange_fs PUBLIC Boost::filesystem)
else()
    message(FATAL_ERROR "Unsupported LAGRANGE_FS_BACKEND: ${LAGRANGE_FS_BACKEND}!")
endif()

# 3. installation
if(LAGRANGE_INSTALL)
    set_target_properties(lagrange_fs PROPERTIES EXPORT_NAME fs)
    lagrange_install(lagrange_fs)
endif()

# 4. unit tests and examples
if(LAGRANGE_UNIT_TESTS)
    add_subdirectory(tests)
endif()
