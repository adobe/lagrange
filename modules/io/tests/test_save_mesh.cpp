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
#include "io_common.h"

#include <lagrange/testing/common.h>
#include <lagrange/io/save_mesh.h>

#include <lagrange/unify_index_buffer.h>

using namespace lagrange;

TEST_CASE("save_mesh", "[io]")
{
    auto cube_indexed = io::testing::create_surfacemesh_cube();
    auto cube = unify_index_buffer(cube_indexed);
    io::SaveOptions opt;
    opt.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh("test_cube.gltf", cube, opt));

    opt.encoding = io::FileEncoding::Binary;
    REQUIRE_NOTHROW(io::save_mesh("test_cube.glb", cube, opt));

    REQUIRE_NOTHROW(io::save_mesh("test_cube.msh", cube));
    REQUIRE_NOTHROW(io::save_mesh("test_cube.obj", cube));
    REQUIRE_NOTHROW(io::save_mesh("test_cube.ply", cube));
}
