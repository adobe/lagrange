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
if(TARGET lagrange::assets)
    return()
endif()

message(STATUS "Lagrange: creating target 'lagrange::assets'")

include(CPM)
CPMAddPackage(
    NAME lagrange-assets
    GITHUB_REPOSITORY adobe/lagrange-assets
    GIT_TAG f3407b0eb8266111c720b28577050e9a8f7901a5
)

add_library(lagrange::assets INTERFACE IMPORTED GLOBAL)
target_include_directories(lagrange::assets INTERFACE "${lagrange-assets_SOURCE_DIR}")
