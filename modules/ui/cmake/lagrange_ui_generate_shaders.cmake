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

# Expects:
# SHADER_DIR
# GENERATED_DIR


#Create shader.inc file
file(GLOB_RECURSE SHADER_FILES "${SHADER_DIR}/*.*")
set(shader_header "${GENERATED_DIR}/shaders.inc.temp")
set(shader_header_final "${GENERATED_DIR}/shaders.inc")

message(STATUS "Generating shader.inc file ...")

file(WRITE ${shader_header} "static std::unordered_map<std::string, std::string> VIRTUAL_SHADERS = {\n")
foreach(shader_file ${SHADER_FILES})

    file(RELATIVE_PATH relative_path ${SHADER_DIR} ${shader_file})

    #get_filename_component(fname ${shader_file} NAME)
    #message(STATUS ${fname} "->" ${subfolder})

    file(READ ${shader_file} content)
    file(APPEND ${shader_header} "{\"${relative_path}\",\n")
    file(APPEND ${shader_header} "R\"(${content})\"}")
    file(APPEND ${shader_header} ",")
    file(APPEND ${shader_header} "\n")
endforeach()

file(APPEND ${shader_header} "};")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${shader_header} ${shader_header_final}
    RESULT_VARIABLE cmp
)

if(cmp EQUAL 0)
    message(STATUS "Shaders are up to date")
    #Same, do nothing
elseif(cmp EQUAL 1)
    #overwrite
    file(REMOVE ${shader_header_final})
    file(RENAME ${shader_header} ${shader_header_final})
    message(STATUS "Updated shaders")
else()
    file(RENAME ${shader_header} ${shader_header_final})
    message(STATUS "Generated ${shader_header_final}")
endif()
