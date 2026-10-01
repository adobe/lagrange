#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

include(sanitizers)

if(MSVC AND USE_SANITIZER)
    # To use sanitizers with MSVC you need to disable incompatible options. See details here:
    # https://learn.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-170#ide-msbuild
    #
    # Please note that there are some issues with using it directly from the IDE, it may work better to
    # run the code from the command-line (and through `ctest` which enables the ASAN_SAVE_DUMPS env variable):
    # https://stackoverflow.com/questions/76781556/visual-studio-22-asan-failed-to-use-and-restart-external-symbolizer
    # https://developercommunity.visualstudio.com/t/Fail-to-use-and-restart-external-symbol/10222443?q=+Failed+to+use+and+restart+external+symbolizer

    # Disable incremental linking
    add_link_options(/INCREMENTAL:NO)

    # Disable RTC flag
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)

    # Do not use program database for debug builds (since it requires incremental linking).
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()
