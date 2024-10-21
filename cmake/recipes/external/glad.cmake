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
if(TARGET glad::glad)
    return()
endif()

message(STATUS "Third-party: creating target 'glad::glad'")

include(FetchContent)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/libigl/libigl-glad.git
    GIT_TAG        ead2d21fd1d9f566d8f9a9ce99ddf85829258c7a
)

FetchContent_MakeAvailable(glad)
add_library(glad::glad ALIAS glad)

set_target_properties(glad PROPERTIES FOLDER ThirdParty)
