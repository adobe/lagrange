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

message(STATUS "Third-party (internal): creating target 'WindingNumber::WindingNumber'")

include(FetchContent)
FetchContent_Declare(
    WindingNumber
    GIT_REPOSITORY https://github.com/jdumas/WindingNumber.git
    GIT_TAG        d0083297a9c3459a61573c0a0b4c0586ea84b727
)

include(tbb)
FetchContent_MakeAvailable(WindingNumber)
set_target_properties(WindingNumber PROPERTIES FOLDER third_party)
