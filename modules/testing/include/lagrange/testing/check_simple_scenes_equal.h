/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/scene/SimpleScene.h>
#include <lagrange/testing/check_meshes_equal.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::testing {

///
/// Check that two simple scenes are bitwise identical.
///
/// Verifies mesh counts, mesh data (via check_meshes_equal), instance counts,
/// mesh indices, and transform matrices (exact match).
/// This is suitable for verifying lossless serialization round-trips.
///
/// @param[in]  a  First simple scene.
/// @param[in]  b  Second simple scene.
///
/// @tparam Scalar     Mesh scalar type.
/// @tparam Index      Mesh index type.
/// @tparam Dimension  Spatial dimension.
///
template <typename Scalar, typename Index, size_t Dimension>
void check_simple_scenes_equal(
    const scene::SimpleScene<Scalar, Index, Dimension>& a,
    const scene::SimpleScene<Scalar, Index, Dimension>& b)
{
    REQUIRE(a.get_num_meshes() == b.get_num_meshes());

    for (Index i = 0; i < a.get_num_meshes(); ++i) {
        check_meshes_equal(a.get_mesh(i), b.get_mesh(i));
        REQUIRE(a.get_num_instances(i) == b.get_num_instances(i));

        for (Index j = 0; j < a.get_num_instances(i); ++j) {
            const auto& inst_a = a.get_instance(i, j);
            const auto& inst_b = b.get_instance(i, j);
            REQUIRE(inst_a.mesh_index == inst_b.mesh_index);
            REQUIRE(inst_a.transform.matrix().isApprox(inst_b.transform.matrix(), Scalar(0)));
        }
    }
}

} // namespace lagrange::testing
