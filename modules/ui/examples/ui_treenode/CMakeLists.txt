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
include(cli11)
lagrange_include_modules(io)

lagrange_add_example(ui_treenode
    main.cpp
)

target_link_libraries(ui_treenode lagrange::ui lagrange::io CLI11::CLI11)

if(LAGRANGE_UI_USE_MDL)
    add_custom_command(TARGET ui_treenode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MDL_DYNAMIC_LIBS}
            $<TARGET_FILE_DIR:ui_treenode>
    )
endif()

