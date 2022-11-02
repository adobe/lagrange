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
if(TARGET Catch2::Catch2)
    return()
endif()

message(STATUS "Third-party (external): creating target 'Catch2::Catch2'")

include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.1.1
    GIT_SHALLOW TRUE
)

option(CATCH_CONFIG_CPP17_STRING_VIEW "Enable support for std::string_view" ON)
FetchContent_MakeAvailable(catch2)

target_compile_features(Catch2 PUBLIC cxx_std_17)

set_target_properties(Catch2 PROPERTIES FOLDER third_party)
set_target_properties(Catch2WithMain PROPERTIES FOLDER third_party)
