/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>
#include <lagrange/io/save_mesh.h>

#include <lagrange/create_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/unify_index_buffer.h>


namespace lagrange::io::testing {

SurfaceMesh32d create_surfacemesh_cube()
{
    std::unique_ptr<TriangleMesh3D> legacy = create_cube();
    return to_surface_mesh_copy<double, uint32_t, TriangleMesh3D>(*legacy);
}

SurfaceMesh32d create_surfacemesh_sphere()
{
    std::unique_ptr<TriangleMesh3D> legacy = create_sphere();
    return to_surface_mesh_copy<double, uint32_t, TriangleMesh3D>(*legacy);
}

}
