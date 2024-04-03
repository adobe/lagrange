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
function(lagrange_install target)
    string(REPLACE "lagrange_" "" module_name ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${module_name})

    include(GNUInstallDirs)
    install(TARGETS ${target}
        EXPORT Lagrange_Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT Lagrange_Runtime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT          Lagrange_Runtime
                NAMELINK_COMPONENT Lagrange_Development   # Requires CMake 3.12
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT Lagrange_Runtime
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                COMPONENT Lagrange_Development
        FILE_SET HEADERS
                COMPONENT Lagrange_Development
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # TODO: Maybe set PUBLIC_HEADER target property (for macOS framework installation)
endfunction()
