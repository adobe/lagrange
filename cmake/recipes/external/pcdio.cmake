#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET pcdio::pcdio)
    return()
endif()

message(STATUS "Third-party (external): creating target 'pcdio::pcdio'")

include(CPM)
block()
    set(CMAKE_FOLDER "third_party")
    CPMAddPackage(
        NAME pcdio
        GIT_REPOSITORY https://github.com/adobe/pcdio.git
        GIT_TAG 2bf236d8a93a52957e4a752968d79926b7e30235
        OPTIONS
            "PCDIO_BUILD_TESTS OFF"
            "PCDIO_BUILD_EXAMPLES OFF"
            "PCDIO_PYTHON OFF"
    )
endblock()

if(TARGET pcdio)
    set_target_properties(pcdio PROPERTIES FOLDER third_party POSITION_INDEPENDENT_CODE ON)
endif()
