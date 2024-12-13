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
if(TARGET WindingNumber::WindingNumber)
    return()
endif()

message(STATUS "Third-party (external): creating target 'WindingNumber::WindingNumber'")

include(tbb)
include(simde)

include(CPM)
CPMAddPackage(
    NAME WindingNumber
    GITHUB_REPOSITORY jdumas/WindingNumber
    GIT_TAG a48b8f555b490afe7aab9159c7daaf83fa2cdf8e
)

set_target_properties(WindingNumber PROPERTIES FOLDER third_party)
