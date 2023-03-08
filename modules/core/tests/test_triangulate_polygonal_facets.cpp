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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/testing/common.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <thread>
#include <numeric>

namespace fs = lagrange::fs;

namespace {

template <typename Scalar, typename Index>
void test_basic()
{
    auto filenames = {
        "blub/blub_quadrangulated.obj",
        "poly/L-plane.obj",
        "poly/hexaSphere.obj",
        "poly/mixedFaring.obj",
        "poly/noisy-sphere.obj",
        "poly/tetris.obj",
        "poly/tetris_2.obj",
        "tilings/semi1.obj",
        "tilings/semi2.obj",
        "tilings/semi3.obj",
        "tilings/semi4.obj",
        "tilings/semi5.obj",
        "tilings/semi6.obj",
        "tilings/semi7.obj",
        "tilings/semi8.obj",
        "non_convex_quad.obj",
    };

    for (fs::path filename : filenames) {
        lagrange::logger().debug("Input path: {}", ("open/core" / filename).string());
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core" / filename);
        lagrange::logger().debug(
            "Loaded mesh with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(!mesh.is_triangle_mesh());

        const Index old_num_vertices = mesh.get_num_vertices();
        lagrange::triangulate_polygonal_facets(mesh);

        // Triangulation does not insert new vertices
        REQUIRE(mesh.get_num_vertices() == old_num_vertices);

        // Because we edit the mesh in place, mesh.is_triangle_mesh() will *not* return true. Once
        // we implement the method SurfaceMesh::compress_if_regular(), we should be able to use it
        // instead.
        bool all_triangles = true;
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            if (mesh.get_facet_size(f) != 3) {
                all_triangles = false;
            }
        }
        REQUIRE(all_triangles);

        lagrange::logger().debug(
            "Mesh after triangulation has {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());

        if (0) {
            // Keep this code for inspection purposes, but do not run.
            fs::path output_filename = filename.stem().string() + "-tri.obj";
            fs::ofstream output_stream(output_filename);
            lagrange::io::save_mesh_obj(output_stream, mesh);
        }
    }
}


template <typename Scalar, typename Index>
void test_2d()
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    auto filenames = {
        "poly/L-plane.obj",
        "poly/tetris.obj",
        "tilings/semi1.obj",
        "non_convex_quad.obj",
    };

    for (fs::path filename : filenames) {
        lagrange::logger().debug("Input path: {}", ("open/core" / filename).string());

        auto mesh = [&] {
            // TODO: Write utils to go from 2d to 3d, and vice-versa (while preserving attributes)
            auto mesh_3d = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core" / filename);
            MeshType mesh_2d(2);
            mesh_2d.add_vertices(mesh_3d.get_num_vertices());
            vertex_ref(mesh_2d) = vertex_view(mesh_3d).leftCols(2);
            mesh_2d.add_hybrid(
                mesh_3d.get_num_facets(),
                [&](Index f) { return mesh_3d.get_facet_size(f); },
                [&](Index f, lagrange::span<Index> t) {
                    auto facet_vertices = mesh_3d.get_facet_vertices(f);
                    std::copy(facet_vertices.begin(), facet_vertices.end(), t.begin());
                });
            return mesh_2d;
        }();
        lagrange::logger().debug(
            "Loaded mesh with {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        REQUIRE(mesh.get_dimension() == 2);
        REQUIRE(!mesh.is_triangle_mesh());

        const Index old_num_vertices = mesh.get_num_vertices();
        lagrange::triangulate_polygonal_facets(mesh);

        // Triangulation does not insert new vertices
        REQUIRE(mesh.get_num_vertices() == old_num_vertices);
        REQUIRE(mesh.is_triangle_mesh());

        lagrange::logger().debug(
            "Mesh after triangulation has {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());

        if (1) {
            // Keep this code for inspection purposes, but do not run.
            fs::path output_filename = filename.stem().string() + "-tri.obj";
            fs::ofstream output_stream(output_filename);
            lagrange::io::save_mesh_obj(output_stream, mesh);
        }
    }
}


template <typename Scalar, typename Index>
void test_triangle()
{
    fs::path filename = "bunny_simple.obj";

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core" / filename);
    lagrange::logger().debug(
        "Loaded mesh with {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());
    REQUIRE(mesh.is_triangle_mesh());

    lagrange::SurfaceMesh<Scalar, Index> copy = mesh;
    lagrange::triangulate_polygonal_facets(mesh);

    // Triangulation should be a no-op, so mesh should share the same buffer before/after
    seq_foreach_named_attribute_read(copy, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        REQUIRE(mesh.has_attribute(name));
        if constexpr (AttributeType::IsIndexed) {
            auto& other = mesh.template get_indexed_attribute<ValueType>(name);
            REQUIRE(other.values().get_all().data() == attr.values().get_all().data());
            REQUIRE(other.indices().get_all().data() == attr.indices().get_all().data());
        } else {
            auto& other = mesh.template get_attribute<ValueType>(name);
            CAPTURE(name);
            la_runtime_assert(other.get_all().data() == attr.get_all().data());
        }
    });
}

template <typename Scalar, typename Index, typename ValueType>
void test_attributes()
{
    using namespace lagrange;

    fs::path filename = "poly/mixedFaringPart.obj";

    auto mesh = [&] {
        LAGRANGE_ZONE_SCOPED
        return lagrange::testing::load_surface_mesh<Scalar, Index>("open/core" / filename);
    }();
    lagrange::logger().debug(
        "Loaded mesh with {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());
    REQUIRE(!mesh.is_triangle_mesh());
    {
        LAGRANGE_ZONE_SCOPED
        mesh.initialize_edges();
    }

    // Create one of each attribute
    auto vid = mesh.template create_attribute<ValueType>("vid", AttributeElement::Vertex);
    auto fid = mesh.template create_attribute<ValueType>("fid", AttributeElement::Facet);
    auto cid = mesh.template create_attribute<ValueType>("cid", AttributeElement::Corner);
    auto eid = mesh.template create_attribute<ValueType>("eid", AttributeElement::Edge);
    auto iid = mesh.template create_attribute<ValueType>("iid", AttributeElement::Indexed);
    auto xid = mesh.template create_attribute<ValueType>("xid", AttributeElement::Value);

    // Initialize attribute values
    {
        LAGRANGE_ZONE_SCOPED

        auto vattr = mesh.template ref_attribute<ValueType>(vid).ref_all();
        std::iota(vattr.begin(), vattr.end(), ValueType(1));
        auto fattr = mesh.template ref_attribute<ValueType>(fid).ref_all();
        std::iota(fattr.begin(), fattr.end(), ValueType(1));
        auto eattr = mesh.template ref_attribute<ValueType>(eid).ref_all();
        std::iota(eattr.begin(), eattr.end(), ValueType(1));
        auto xattr = mesh.template ref_attribute<ValueType>(xid).ref_all();
        std::iota(xattr.begin(), xattr.end(), ValueType(1));

        // For the indexed attribute, we also need to insert additional values
        auto& attr = mesh.template ref_indexed_attribute<ValueType>(iid);
        attr.values().insert_elements(mesh.get_num_corners());
        auto attr_indices = attr.indices().ref_all();
        std::iota(attr_indices.begin(), attr_indices.end(), Index(0));

        // Attach vertex indices as corner/indexed attribute values
        auto cattr = mesh.template ref_attribute<ValueType>(cid).ref_all();
        auto attr_values = attr.values().ref_all();
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            for (Index c = mesh.get_facet_corner_begin(f); c < mesh.get_facet_corner_end(f); ++c) {
                const Index v = mesh.get_corner_vertex(c);
                cattr[c] = vattr[v];
                attr_values[c] = vattr[v];
            }
        }
    }

    lagrange::triangulate_polygonal_facets(mesh);

    // We can only "verify" correctness information for corner attributes and indexed attribute
    // indices. Verifying the correctness of the facet remapping basically means reimplementing this
    // remapping manually in this unit test, which is kind of pointless I guess?

    LAGRANGE_ZONE_SCOPED

    auto vattr = mesh.template get_attribute<ValueType>(vid).get_all();
    auto cattr = mesh.template get_attribute<ValueType>(cid).get_all();
    auto& iattr = mesh.template get_indexed_attribute<ValueType>(iid);
    auto iattr_values = iattr.values().get_all();
    auto iattr_indices = iattr.indices().get_all();
    bool all_match = true;
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        for (Index c = mesh.get_facet_corner_begin(f); c < mesh.get_facet_corner_end(f); ++c) {
            const Index v = mesh.get_corner_vertex(c);
            REQUIRE(cattr[c] == vattr[v]);
            REQUIRE(iattr_values[iattr_indices[c]] == vattr[v]);
        }
    }
    REQUIRE(all_match);

    auto& eattr = mesh.template get_attribute<ValueType>(eid);
    bool has_zeros = false;
    REQUIRE(eattr.get_default_value() == ValueType(0));
    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
        has_zeros |= (eattr.get(e, 0) == ValueType(0));
    }
    REQUIRE(has_zeros);

    seq_foreach_attribute_read<AttributeElement::Edge>(mesh, [&](auto&& attr) {
        REQUIRE(attr.get_num_elements() == mesh.get_num_edges());
    });

    LAGRANGE_FRAME_MARK
}

} // namespace

TEST_CASE("triangulate_polygonal_facets: basic", "[core]")
{
#define LA_X_basic(_, Scalar, Index) test_basic<Scalar, Index>();
    LA_SURFACE_MESH_X(basic, 0)
}

TEST_CASE("triangulate_polygonal_facets: 2d", "[core]")
{
#define LA_X_2d(_, Scalar, Index) test_2d<Scalar, Index>();
    LA_SURFACE_MESH_X(2d, 0)
}

TEST_CASE("triangulate_polygonal_facets: triangle", "[core]")
{
#define LA_X_triangle(_, Scalar, Index) test_triangle<Scalar, Index>();
    LA_SURFACE_MESH_X(triangle, 0)
}

TEST_CASE("triangulate_polygonal_facets: attributes", "[core]")
{
#define LA_X_attributes(ValueType, Scalar, Index) test_attributes<Scalar, Index, ValueType>();
#define LA_X_attributes_aux(_, ValueType) LA_SURFACE_MESH_X(attributes, ValueType)
    LA_ATTRIBUTE_X(attributes_aux, 0)
}

// TODO: Test removal degenerate facets, once we allow sizes <= 2
// TODO: Test with 2d meshes
