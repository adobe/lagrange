/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

// An alternative to injecting asset locations as command line macros,
// is using a generated header.
#ifdef LA_TESTING_USE_CONFIG
#include <lagrange/testing/config.h>
#endif

#include <lagrange/io/load_mesh.h>

#include <catch2/catch.hpp>

namespace lagrange {
namespace testing {

///
/// Gets the absolute path to a file in the test data directory. This function asserts that the file
/// exists.
///
/// @param[in]  relative_path  Relative path to the file.
///
/// @return     Absolute path to the file.
///
fs::path get_data_path(const fs::path &relative_path);

///
/// Loads a mesh from the test data directory.
///
/// @param[in]  relative_path  Relative path of the file to load.
///
/// @tparam     MeshType       Mesh type.
///
/// @return     A unique_ptr to the newly allocated mesh.
///
template <typename MeshType>
std::unique_ptr<MeshType> load_mesh(const fs::path &relative_path)
{
    auto result = lagrange::io::load_mesh<MeshType>(get_data_path(relative_path));
    REQUIRE(result);
    return result;
}

} // namespace testing
} // namespace lagrange
