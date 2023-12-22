#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET StackWalker::StackWalker)
    return()
endif()

message(STATUS "Third-party (external): creating target 'StackWalker::StackWalker'")

option(StackWalker_DISABLE_TESTS  "Disable tests" ON)

include(CPM)
CPMAddPackage(
    NAME StackWalker
    GITHUB_REPOSITORY JochenKalmbach/StackWalker
    GIT_TAG 5b0df7a4db8896f6b6dc45d36e383c52577e3c6b
    DOWNLOAD_ONLY YES
)

add_library(StackWalker STATIC ${StackWalker_SOURCE_DIR}/Main/StackWalker/StackWalker.cpp)
add_library(StackWalker::StackWalker ALIAS StackWalker)

target_include_directories(StackWalker PUBLIC ${StackWalker_SOURCE_DIR}/Main/StackWalker)

set_target_properties(StackWalker PROPERTIES FOLDER third_party)
