/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

namespace {

template <typename Scalar, typename Index, typename ValueType>
void test_views_generic()
{
    // Generic attribute view
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.add_quads(2);
        mesh.add_triangles(2);

        for (size_t num_channels : {1, 3}) {
            std::string name = "foo";
            mesh.template create_attribute<ValueType>(
                name,
                lagrange::AttributeElement::Vertex,
                lagrange::AttributeUsage::Vector,
                num_channels);
            auto M = lagrange::attribute_matrix_ref<ValueType>(mesh, name);
            ValueType cnt(0);
            M = M.unaryExpr([&](auto) { return cnt++; });
            REQUIRE(
                lagrange::attribute_matrix_view<ValueType>(mesh, name) ==
                matrix_view(mesh.template get_attribute<ValueType>(name)));

            if (num_channels != 1) {
                LA_REQUIRE_THROWS(lagrange::attribute_vector_view<ValueType>(mesh, name));
            } else {
                auto A = lagrange::attribute_vector_view<ValueType>(mesh, name);
                auto B = vector_view(mesh.template get_attribute<ValueType>(name));
                REQUIRE(A[1] == B[1]);
            }
            mesh.delete_attribute(name);
        }
    }
}

template <typename Scalar, typename Index>
void test_views_all()
{
#define LA_X_test_views_generic(_, ValueType) test_views_generic<Scalar, Index, ValueType>();
    LA_ATTRIBUTE_X(test_views_generic, 0);

    // Regular mesh vertex/facet view
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(4);

        REQUIRE(vertex_view(mesh) == matrix_view(mesh.get_vertex_to_position()));
        REQUIRE(facet_view(mesh) == reshaped_view(mesh.get_corner_to_vertex(), 3));
    }

    // Polygonal mesh vertex/facet view
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.add_quads(2);
        mesh.add_triangles(2);

        REQUIRE(vertex_view(mesh) == matrix_view(mesh.get_vertex_to_position()));
        LA_REQUIRE_THROWS(facet_view(mesh));
    }
}

} // namespace

TEST_CASE("views", "[core]")
{
#define LA_X_test_views_all(_, Scalar, Index) test_views_all<Scalar, Index>();
    LA_SURFACE_MESH_X(test_views_all, 0)
}
