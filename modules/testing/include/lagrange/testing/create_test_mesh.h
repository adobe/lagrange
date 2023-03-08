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
#pragma once
#include <lagrange/SurfaceMesh.h>

namespace lagrange::testing {

/**
 * Options for mesh creation.
 */
struct CreateOptions
{
    bool with_indexed_uv = true; ///< Initialize an indexed uv attribute.
    bool with_indexed_normal = true; ///< Initialize an indexed normal attribute.
};

/**
 * Create a triangulated cube mesh.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> create_test_cube(CreateOptions options = {});

/**
 * Create a triangulated sphere mesh.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> create_test_sphere(CreateOptions options = {});

} // namespace lagrange::testing
