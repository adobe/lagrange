#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET poissonrecon::poissonrecon)
    return()
endif()

message(STATUS "Third-party (external): creating target 'poissonrecon::poissonrecon'")

include(CPM)
CPMAddPackage(
    NAME poissonrecon
    GITHUB_REPOSITORY mkazhdan/PoissonRecon
    GIT_TAG 23446519aa82d625372935b76be3dab3b17b572d
)

add_library(poissonrecon::poissonrecon INTERFACE IMPORTED GLOBAL)
target_include_directories(poissonrecon::poissonrecon SYSTEM INTERFACE "${poissonrecon_SOURCE_DIR}/Src")
target_compile_definitions(poissonrecon::poissonrecon INTERFACE "SANITIZED_PR")
