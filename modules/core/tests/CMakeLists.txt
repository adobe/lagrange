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
lagrange_add_test()

include(mikktspace)
target_link_libraries(test_lagrange_core PUBLIC mikktspace::mikktspace)
target_compile_definitions(test_lagrange_core PUBLIC LAGRANGE_WITH_MIKKTSPACE)

if(LAGRANGE_USE_PCH)
    # Use precompiled headers in core tests since there are a lot of cpp files in core tests.
    # Check the timing if you want to add this to your own module, it may or may not be worth it.
    target_link_libraries(test_lagrange_core PRIVATE lagrange_testing_pch)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
