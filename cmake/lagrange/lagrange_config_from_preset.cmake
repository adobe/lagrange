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

# Simple script that outputs the single config build type (if any) given by a preset
if(CMAKE_SCRIPT_MODE_FILE)
    if(DEFINED CMAKE_ARGV3)
        execute_process(COMMAND ${CMAKE_COMMAND} --preset ${CMAKE_ARGV3} -N OUTPUT_VARIABLE output_log COMMAND_ERROR_IS_FATAL ANY)
        if(output_log MATCHES "CMAKE_BUILD_TYPE=\"([a-zA-Z]+)\"")
            execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Could not find CMAKE_BUILD_TYPE from preset")
        endif()
    endif()
endif()
