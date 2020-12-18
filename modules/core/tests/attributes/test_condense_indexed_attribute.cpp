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
#include <Eigen/Core>
#include <lagrange/testing/common.h>

#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

TEST_CASE("CondenseIndexedAttribute", "[attribute][condense][indexed]")
{
    using namespace lagrange;

    SECTION("uv from corner attribute 2")
    {
        Vertices2D vertices(4, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = create_mesh(vertices, facets);

        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = MeshType::AttributeArray;
        const std::string attr_name = "uv";
        AttributeArray attr(6, 2);

        SECTION("Consistent UV")
        {
            attr << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0;
            mesh->add_corner_attribute(attr_name);
            mesh->import_corner_attribute(attr_name, attr);
            map_corner_attribute_to_indexed_attribute(*mesh, attr_name);
            mesh->remove_corner_attribute(attr_name);

            REQUIRE(mesh->is_uv_initialized());

            const auto& uv = mesh->get_uv();
            const auto& uv_indices = mesh->get_uv_indices();
            REQUIRE(uv.rows() == 4);
            REQUIRE(uv_indices.rows() == 2);
            for (auto i : range(2)) {
                REQUIRE(uv(uv_indices(i, 0), 0) == vertices(facets(i, 0), 0));
                REQUIRE(uv(uv_indices(i, 1), 0) == vertices(facets(i, 1), 0));
                REQUIRE(uv(uv_indices(i, 2), 0) == vertices(facets(i, 2), 0));

                REQUIRE(uv(uv_indices(i, 0), 1) == vertices(facets(i, 0), 1));
                REQUIRE(uv(uv_indices(i, 1), 1) == vertices(facets(i, 1), 1));
                REQUIRE(uv(uv_indices(i, 2), 1) == vertices(facets(i, 2), 1));
            }
        }

        SECTION("Inconsistent UV")
        {
            attr << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.1, 1.0, 1.0, 0.0, 1.0, 1.0;
            mesh->add_corner_attribute(attr_name);
            mesh->import_corner_attribute(attr_name, attr);
            map_corner_attribute_to_indexed_attribute(*mesh, attr_name);
            mesh->remove_corner_attribute(attr_name);

            REQUIRE(mesh->is_uv_initialized());

            const auto& uv = mesh->get_uv();
            REQUIRE(uv.rows() == 5);
            const auto& uv_indices = mesh->get_uv_indices();
            REQUIRE(uv_indices.rows() == 2);
        }
    }
}
