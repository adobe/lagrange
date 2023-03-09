/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/common.h>
#include <lagrange/mesh_convert.h>

void test_to_surface_mesh()
{
    using Scalar = double;
    using Index = uint32_t;
    using Triangles32 = Eigen::Matrix<uint32_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using TriangleMesh3D32 = lagrange::Mesh<lagrange::Vertices3D, Triangles32>;
    using MeshType = TriangleMesh3D32;

    // Trying to wrap a temporary should not compile
    auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(MeshType());
}
