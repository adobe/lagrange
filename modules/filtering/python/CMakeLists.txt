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

if(NOT SKBUILD)
    return()
endif()

# TODO: We should really change this to `find_package(MKL)` installed by pip rather than download
# and install it ourselves when building Python wheels.
if(TARGET MKL::MKL)
    target_link_libraries(lagrange_python PRIVATE MKL::MKL)
    # if(SKBUILD AND LINUX)
    #     target_compile_definitions(lagrange_python PRIVATE LAGRANGE_USE_MKL_SDL)
    # endif()

    include(lagrange_runtime_dependencies)
    lagrange_get_dependencies(mkl_dependencies MKL::MKL)
    foreach(dep IN ITEMS ${mkl_dependencies})
        get_target_property(dep_type ${dep} TYPE)
        set(installable_types MODULE_LIBRARY SHARED_LIBRARY)
        if(dep_type IN_LIST installable_types)
            install(IMPORTED_RUNTIME_ARTIFACTS ${dep}
                DESTINATION ${SKBUILD_PLATLIB_DIR}/lagrange
                COMPONENT Lagrange_Python_Runtime
            )
        endif()
    endforeach()
endif()
