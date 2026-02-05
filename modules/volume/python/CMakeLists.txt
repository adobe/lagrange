#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
lagrange_add_python_binding()

target_link_libraries(lagrange_python PRIVATE OpenVDB::nanovdb)

if(SKBUILD)
    foreach(vdb_target IN ITEMS OpenVDB::openvdb OpenVDB::nanovdb boost_container boost_iostreams boost_random)
        get_target_property(_aliased ${vdb_target} ALIASED_TARGET)

        if(_aliased)
            set(vdb_target ${_aliased})
        else()
            set(vdb_target ${vdb_target})
        endif()

        install(TARGETS ${vdb_target}
            DESTINATION ${SKBUILD_PLATLIB_DIR}/lagrange
            COMPONENT Lagrange_Python_Runtime
        )
    endforeach()
endif()
