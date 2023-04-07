#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# Set TBB_SANITIZE based on the USE_SANITIZER option
set(LAGRANGE_TBB_SANITIZE "")
if(USE_SANITIZER)
    if(UNIX)
        if(USE_SANITIZER MATCHES "([Aa]ddress);([Uu]ndefined)"
            OR USE_SANITIZER MATCHES "([Uu]ndefined);([Aa]ddress)")
            set(LAGRANGE_TBB_SANITIZE "address -fno-omit-frame-pointer")
        elseif(USE_SANITIZER MATCHES "([Aa]ddress)")
            set(LAGRANGE_TBB_SANITIZE "address -fno-omit-frame-pointer")
        elseif(USE_SANITIZER MATCHES "([Mm]emory([Ww]ith[Oo]rigins)?)")
            set(LAGRANGE_TBB_SANITIZE "memory")
        elseif(USE_SANITIZER MATCHES "([Tt]hread)")
            set(LAGRANGE_TBB_SANITIZE "thread")
        elseif(USE_SANITIZER MATCHES "([Ll]eak)")
            set(LAGRANGE_TBB_SANITIZE "leak")
        endif()
    elseif(MSVC)
        if(USE_SANITIZER MATCHES "([Aa]ddress)")
            set(LAGRANGE_TBB_SANITIZE "address -fno-omit-frame-pointer")
        else()
        endif()
    endif()

endif()

set(TBB_SANITIZE ${LAGRANGE_TBB_SANITIZE} CACHE STRING "Sanitizer parameter passed to compiler/linker" FORCE)
