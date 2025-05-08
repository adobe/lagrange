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

block()
    set(BUILD_SHARED_LIBS OFF)
    include(CPM)
    CPMAddPackage(
        NAME glad
        GIT_REPOSITORY https://github.com/libigl/libigl-glad.git
        GIT_TAG        3b1d0005d1c4a07c0a3d23a0c3b38c7eedf027e0
        )
endblock()

add_library(glad::glad ALIAS glad)

set_target_properties(glad PROPERTIES FOLDER third_party)
