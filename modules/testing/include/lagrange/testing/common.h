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
    #include <lagrange/testing/public_config.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/io/legacy/load_mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/testing/api.h>
#include <lagrange/utils/assert.h>

#include <catch2/catch_test_macros.hpp>

///
/// Convenience macro wrapping around Catch2's REQUIRE_THROWS macro. We disable triggering debugger
/// breakpoint on assert failure when the expected behavior of the expression being called is to
/// throw an exception.
///
/// @param      ...   Expression to test.
///
/// @return     A void expression.
///
#define LA_REQUIRE_THROWS(...)                                  \
    do {                                                        \
        bool __was_enabled = lagrange::is_breakpoint_enabled(); \
        lagrange::set_breakpoint_enabled(false);                \
        REQUIRE_THROWS(__VA_ARGS__);                            \
        lagrange::set_breakpoint_enabled(__was_enabled);        \
    } while (0)

///
/// Convenience macro wrapping around Catch2's REQUIRE_THROWS_WITH macro. We disable triggering
/// debugger breakpoint on assert failure when the expected behavior of the expression being called
/// is to throw an exception.
///
/// @param      x     Expression to test.
/// @param      y     Exception expected to be thrown.
///
/// @return     A void expression.
///
#define LA_REQUIRE_THROWS_WITH(x, y)                            \
    do {                                                        \
        bool __was_enabled = lagrange::is_breakpoint_enabled(); \
        lagrange::set_breakpoint_enabled(false);                \
        REQUIRE_THROWS_WITH(x, y);                              \
        lagrange::set_breakpoint_enabled(__was_enabled);        \
    } while (0)

///
/// Convenience macro wrapping around Catch2's CHECK_THROWS macro. We disable triggering debugger
/// breakpoint on assert failure when the expected behavior of the expression being called is to
/// throw an exception.
///
/// @param      ...   Expression to test.
///
/// @return     A void expression.
///
#define LA_CHECK_THROWS(...)                                    \
    do {                                                        \
        bool __was_enabled = lagrange::is_breakpoint_enabled(); \
        lagrange::set_breakpoint_enabled(false);                \
        CHECK_THROWS(__VA_ARGS__);                              \
        lagrange::set_breakpoint_enabled(__was_enabled);        \
    } while (0)


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
LA_TESTING_API fs::path get_data_path(const fs::path& relative_path);

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
std::unique_ptr<MeshType> load_mesh(const fs::path& relative_path)
{
    auto result = lagrange::io::load_mesh<MeshType>(get_data_path(relative_path));
    REQUIRE(result);
    return result;
}
extern template LA_TESTING_API std::unique_ptr<TriangleMesh3D> load_mesh(const fs::path&);
extern template LA_TESTING_API std::unique_ptr<QuadMesh3D> load_mesh(const fs::path&);

///
/// Load a mesh from test data directory as a `SurfaceMesh`.
///
/// @param[in]  relative_path  Relative path of the file to load.
///
/// @tparam     Scalar       Scalar type.
/// @tparam     Index        Index type.
///
/// @return     A `SurfaceMesh` obj.
///
/// @note  Only obj mesh are supported for now.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> load_surface_mesh(const fs::path& relative_path)
{
    auto full_path = get_data_path(relative_path);
    REQUIRE(lagrange::fs::exists(full_path));
    return lagrange::io::load_mesh<SurfaceMesh<Scalar, Index>>(full_path);
}

///
/// Set up MKL Conditional Numerical Reproducibility to ensure maximum compatibility between
/// devices. This is only called before setting up unit tests that depend on reproducible numerical
/// results. Otherwise, the behavior can be controlled by the environment variable MKL_CBWR. See [1]
/// for additional information. This function has no effect if Lagrange is compiled without MKL.
///
/// [1]:
/// https://software.intel.com/content/www/us/en/develop/articles/introduction-to-the-conditional-numerical-reproducibility-cnr.html
///
LA_TESTING_API void setup_mkl_reproducibility();

} // namespace testing
} // namespace lagrange
