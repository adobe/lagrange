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

include(FetchContent)
FetchContent_Declare(
    WindingNumber
    GIT_REPOSITORY https://github.com/sideeffects/WindingNumber.git
    GIT_TAG        1e6081e52905575d8e98fb8b7c0921274a18752f
)

FetchContent_MakeAvailable(WindingNumber)

file(GLOB SRC_FILES
    "${windingnumber_SOURCE_DIR}/*.cpp"
    "${windingnumber_SOURCE_DIR}/*.h"
)
add_library(WindingNumber ${SRC_FILES})
add_library(WindingNumber::WindingNumber ALIAS WindingNumber)

target_include_directories(WindingNumber PUBLIC ${windingnumber_SOURCE_DIR})

include(tbb)
target_link_libraries(WindingNumber PUBLIC TBB::tbb)

set_target_properties(WindingNumber PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(WindingNumber PROPERTIES FOLDER third_party)
