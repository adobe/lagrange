# Source: https://github.com/StableCoder/cmake-scripts
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2018 by George Cave - gcave@stablecoder.ca
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# This file has been modified by Adobe.
#
# All modifications are Copyright 2020 Adobe.

set(USE_SANITIZER
    ""
    CACHE
      STRING
      "Compile with a sanitizer. Options are: Address, Memory, MemoryWithOrigins, Undefined, Thread, Leak, 'Address;Undefined'"
)

function(append value)
  foreach(variable ${ARGN})
    set(${variable}
        "${${variable}} ${value}"
        PARENT_SCOPE)
  endforeach(variable)
endfunction()

if(USE_SANITIZER)
  if(UNIX)
    append("-fno-omit-frame-pointer" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)

    if(uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
      append("-O1" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    endif()

    if(USE_SANITIZER MATCHES "([Aa]ddress);([Uu]ndefined)"
       OR USE_SANITIZER MATCHES "([Uu]ndefined);([Aa]ddress)")
      message(STATUS "Building with Address, Undefined sanitizers")
      append("-fsanitize=address,undefined" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    elseif(USE_SANITIZER MATCHES "([Aa]ddress)")
      # Optional: -fno-optimize-sibling-calls -fsanitize-address-use-after-scope
      message(STATUS "Building with Address sanitizer")
      append("-fsanitize=address" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    elseif(USE_SANITIZER MATCHES "([Mm]emory([Ww]ith[Oo]rigins)?)")
      # Optional: -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2
      append("-fsanitize=memory" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
      if(USE_SANITIZER MATCHES "([Mm]emory[Ww]ith[Oo]rigins)")
        message(STATUS "Building with MemoryWithOrigins sanitizer")
        append("-fsanitize-memory-track-origins" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
      else()
        message(STATUS "Building with Memory sanitizer")
      endif()
    elseif(USE_SANITIZER MATCHES "([Uu]ndefined)")
      message(STATUS "Building with Undefined sanitizer")
      append("-fsanitize=undefined" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
      if(EXISTS "${BLACKLIST_FILE}")
        append("-fsanitize-blacklist=${BLACKLIST_FILE}" CMAKE_C_FLAGS
               CMAKE_CXX_FLAGS)
      endif()
    elseif(USE_SANITIZER MATCHES "([Tt]hread)")
      message(STATUS "Building with Thread sanitizer")
      append("-fsanitize=thread" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    elseif(USE_SANITIZER MATCHES "([Ll]eak)")
      message(STATUS "Building with Leak sanitizer")
      append("-fsanitize=leak" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    else()
      message(
        FATAL_ERROR "Unsupported value of USE_SANITIZER: ${USE_SANITIZER}")
    endif()
  elseif(MSVC)
    if(USE_SANITIZER MATCHES "([Aa]ddress)")
      message(STATUS "Building with Address sanitizer")
      # Do use AddressSanitizer you need to disable incompatible options. See details here:
      # https://learn.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-170#ide-msbuild
      #
      # Please note that there are some issues with using it directly from the IDE, it may work better to
      # run the code from the command-line (and through `ctest` which enables the ASAN_SAVE_DUMPS env variable):
      # https://stackoverflow.com/questions/76781556/visual-studio-22-asan-failed-to-use-and-restart-external-symbolizer
      # https://developercommunity.visualstudio.com/t/Fail-to-use-and-restart-external-symbol/10222443?q=+Failed+to+use+and+restart+external+symbolizer
      append("/fsanitize=address" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)

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
    else()
      message(
        FATAL_ERROR
          "This sanitizer not yet supported in the MSVC environment: ${USE_SANITIZER}"
      )
    endif()
  else()
    message(FATAL_ERROR "USE_SANITIZER is not supported on this platform.")
  endif()

endif()
