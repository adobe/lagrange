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
if(TARGET shape_gradient_domain::shape_gradient_domain)
    return()
endif()

message(STATUS "Third-party (external): creating target 'shape_gradient_domain::shape_gradient_domain'")

include(CPM)
CPMAddPackage(
    NAME shape_gradient_domain
    GITHUB_REPOSITORY mkazhdan/ShapeGradientDomain
    GIT_TAG 6e748544d9096b4dbb0f8c1362ac626c26279c57
)

add_library(shape_gradient_domain::shape_gradient_domain INTERFACE IMPORTED GLOBAL)
target_include_directories(shape_gradient_domain::shape_gradient_domain
    SYSTEM INTERFACE "${shape_gradient_domain_SOURCE_DIR}"
)

include(misha)
target_link_libraries(shape_gradient_domain::shape_gradient_domain INTERFACE misha::misha)
