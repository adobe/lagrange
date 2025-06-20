#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET Python::Module)
    return()
endif()

set(Python_FIND_VIRTUALENV FIRST)

# Python ABI to search for, according to https://peps.python.org/pep-3149/
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.30)
    # pydebug (d), pymalloc (m), unicode (u) and gil_disabled (t)
    set(Python_FIND_ABI "ANY" "ANY" "ANY" "OFF")
else()
    # pydebug (d), pymalloc (m), unicode (u)
    set(Python_FIND_ABI "ANY" "ANY" "ANY")
endif()

find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)
