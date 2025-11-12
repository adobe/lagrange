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

if(TARGET cpptrace::cpptrace)
    return()
endif()

message(STATUS "Third-party (external): creating target 'cpptrace::cpptrace'")

include(CPM)
CPMAddPackage(
    NAME cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.4
)

if(TARGET cpptrace-lib)
    set_target_properties(cpptrace-lib PROPERTIES FOLDER third_party)
endif()
