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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_triangles.h>

namespace mesh_io_helper {

template <typename DerivedV, typename DerivedF>
void process_mesh_in_raw_eigen_type(DerivedV& vertices, DerivedF& facets)
{
    using namespace lagrange;
    auto in_mesh = wrap_with_mesh(vertices, facets);
    REQUIRE(vertices.data() == in_mesh->get_vertices().data());
    REQUIRE(facets.data() == in_mesh->get_facets().data());
    auto out_mesh = remove_topologically_degenerate_triangles(*in_mesh);
}

template <typename DerivedV, typename DerivedF>
void process_mesh_in_plain_object_base(
    Eigen::PlainObjectBase<DerivedV>& vertices,
    Eigen::PlainObjectBase<DerivedF>& facets)
{
    using namespace lagrange;
    auto in_mesh = wrap_with_mesh(vertices, facets);
    REQUIRE(vertices.data() == in_mesh->get_vertices().data());
    REQUIRE(facets.data() == in_mesh->get_facets().data());
    auto out_mesh = remove_topologically_degenerate_triangles(*in_mesh);
}

} // namespace mesh_io_helper

TEST_CASE("wrap_with_mesh", "[mesh][io][wrap]")
{
    using namespace lagrange;

    SECTION("RowMajor")
    {
        using VertexArray = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
        using FacetArray = Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor>;

        VertexArray vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        FacetArray facets(1, 3);
        facets << 0, 1, 2;

        mesh_io_helper::process_mesh_in_raw_eigen_type(vertices, facets);
        mesh_io_helper::process_mesh_in_plain_object_base(vertices, facets);
    }

    SECTION("ColMajor")
    {
        using VertexArray = Eigen::Matrix<float, Eigen::Dynamic, 3>;
        using FacetArray = Eigen::Matrix<uint64_t, Eigen::Dynamic, 3>;

        VertexArray vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        FacetArray facets(1, 3);
        facets << 0, 1, 2;

        mesh_io_helper::process_mesh_in_raw_eigen_type(vertices, facets);
        mesh_io_helper::process_mesh_in_plain_object_base(vertices, facets);
    }

    SECTION("Eigen blocks")
    {
        using Scalar = float;
        using Index = int;
        using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
        using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3>;

        VertexArray vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;

        FacetArray facets(3, 3);
        facets << 0, 1, 2, 0, 0, 1, 1, 1, 2;

        auto vertex_block = vertices.topRows(3);
        auto facet_block = facets.topRows(1);

        REQUIRE(vertex_block.rows() == 3);
        REQUIRE(facet_block.rows() == 1);

        // The following two lines should not compile.  It prevents users from
        // using Eigen::Block as VertexArray or FacetArray.
        //
        // mesh_io_helper::process_mesh_in_raw_eigen_type(vertex_block, facet_block);
        // mesh_io_helper::process_mesh_in_plain_object_base(vertex_block, facet_block);

        // The following should not compile.  lagrange::wrap_with_mesh should
        // NOT bind with rvalues.
        //
        // auto mesh = lagrange::wrap_with_mesh(
        //         vertex_block.eval(),
        //         facet_block.eval());
    }
}
