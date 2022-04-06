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
function(lagrange_download_data)
    get_property(lagrange_source_dir GLOBAL PROPERTY __lagrange_source_dir)

    # Data folder path can be overriden by the user
    set(LAGRANGE_DATA_FOLDER "${lagrange_source_dir}/data"
        CACHE PATH
        "Where should Lagrange download and look for test data?"
    )
    message(STATUS "Using test data folder: ${LAGRANGE_DATA_FOLDER}")

    if(LAGRANGE_EXTERNAL_ONLY)
        # For now we use CMake ExternalProject to git clone the repository containing the test data.
        # In the future we might switch to another provider and download each file directy.
        include(ExternalProject)
        include(FetchContent)
        ExternalProject_Add(
            lagrange-test-data
            PREFIX "${FETCHCONTENT_BASE_DIR}/lagrange-test-data"
            SOURCE_DIR ${LAGRANGE_DATA_FOLDER}
            GIT_REPOSITORY https://github.com/adobe/lagrange-test-data.git
            GIT_TAG 5d2f3ac3ee84fe7ab044a3a7fb0120e6ee6d3aa4
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
        )

        # Create a dummy target for convenience
        add_custom_target(lagrange_download_data)
        set_target_properties(lagrange_download_data PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Utils")
    else()
        # Corp version, we automatically download test data via Artifactory
        file(STRINGS "${lagrange_source_dir}/scripts/manifest.txt" FILE_LIST)
        unset(DATA_DEPENDS)
        foreach(entry IN ITEMS ${FILE_LIST})
            separate_arguments(entry UNIX_COMMAND ${entry})
            list(GET entry 0 local_relpath)
            list(GET entry 1 remote_path)
            list(GET entry 2 checksum)
            set(local_path "${LAGRANGE_DATA_FOLDER}/${local_relpath}")

            add_custom_command(
                OUTPUT
                    ${local_path}
                COMMAND
                    ${CMAKE_COMMAND} -P
                        "${lagrange_source_dir}/cmake/recipes/internal/artifactory.cmake"
                        "https://artifactory.corp.adobe.com/artifactory/generic-lagrange-test/main"
                        "${LAGRANGE_DATA_FOLDER}"
                        "${local_relpath}"
                        "${remote_path}"
                        "${checksum}"
                        "${LAGRANGE_ARTIFACTORY_KEYFILE}"
                COMMENT
                    "Downloading file ${local_path}"
                VERBATIM
            )

            list(APPEND DATA_DEPENDS "${local_path}")
        endforeach()

        # Mark all binary data as dependencies of our target
        add_custom_target(
            lagrange_download_data
            DEPENDS ${DATA_DEPENDS}
        )
        set_target_properties(lagrange_download_data PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Utils")

        # Tell the build system to re-run CMake when those files are updated
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            ${lagrange_source_dir}/scripts/manifest.txt
            ${lagrange_source_dir}/cmake/internal/artifactory.cmake
        )
    endif()
endfunction()
