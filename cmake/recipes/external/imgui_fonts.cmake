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

# The fonts repo does not add any target by default, but it does add this function
if(NOT COMMAND fonts_add_font)

    message(STATUS "Third-party (external): creating target 'imgui::fonts'")

    include(CPM)
    CPMAddPackage(
        NAME imgui_fonts
        GITHUB_REPOSITORY HasKha/imgui-fonts
        GIT_TAG 24e4eca2d0e51d5214780391a94663a08f884762
    )

endif()

block()
    set(BUILD_SHARED_LIBS OFF)
    fonts_add_font(fontawesome6)
    fonts_add_font(source_sans_pro_regular)
endblock()

set_target_properties(fonts_fontawesome6 PROPERTIES FOLDER third_party)
set_target_properties(fonts_source_sans_pro_regular PROPERTIES FOLDER third_party)
