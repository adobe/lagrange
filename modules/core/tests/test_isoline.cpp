/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/compute_components.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/isoline.h>
#include <lagrange/orientation.h>
#include <lagrange/topology.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <tbb/task_arena.h>

#include <random>

namespace {

// create a square mesh ([0 1] x [0 1]).
// n and m are the number of vertices in x and y
// num_dims: if 2 creates a 2D mesh. Otherwise a 3-D one.
// delta is for perturbing the mesh slightly
template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index>
create_grid(const Index n, const Index m, const Index num_dims, const Scalar delta = 0)
{
    la_runtime_assert(num_dims == 2 || num_dims == 3);
    const Index num_vertices = n * m;
    const Index num_facets = (n - 1) * (m - 1) * 2;
    lagrange::SurfaceMesh<Scalar, Index> mesh(num_dims);
    mesh.add_vertices(num_vertices);
    mesh.add_triangles(num_facets);
    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);
    for (auto i : lagrange::range(n)) {
        for (auto j : lagrange::range(m)) {
            const Scalar iscaled = Scalar(i) / (n - 1);
            const Scalar jscaled = Scalar(j) / (m - 1);
            LA_IGNORE_ARRAY_BOUNDS_BEGIN
            Eigen::RowVector3<Scalar> pt(iscaled, jscaled, 0);
            vertices.row(j * n + i) = pt.head(num_dims);
            LA_IGNORE_ARRAY_BOUNDS_END
        }
    }
    for (auto i : lagrange::range(n - 1)) {
        for (auto j : lagrange::range(m - 1)) {
            const Index j1 = j + 1;
            const Index i1 = i + 1;
            facets.row((j * (n - 1) + i) * 2 + 0) << j * n + i, j * n + i1, j1 * n + i;
            facets.row((j * (n - 1) + i) * 2 + 1) << j * n + i1, j1 * n + i1, j1 * n + i;
        }
    }

    // Only perturb in x and y
    // otherwise the analytical ellipse perimeter computations will not be accurate
    const Scalar bound = delta * (1. / std::max(n, m));
    std::mt19937 gen;
    std::uniform_real_distribution<Scalar> dist(-bound, bound);
    vertices.leftCols(2) = vertices.leftCols(2).unaryExpr([&](Scalar x) { return x + dist(gen); });

    return mesh;
};

} // namespace


TEST_CASE("trim_by_isoline basic", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;
    Index num_dims;
    Index n = 20;
    Index m = 35;
    Scalar delta = 0.2f;

    SECTION("2D")
    {
        num_dims = 2;
    }
    SECTION("3D")
    {
        num_dims = 3;
    }

    auto mesh = create_grid(n, m, num_dims, delta);
    auto id = mesh.create_attribute<double>(
        "random_attribute",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar);
    auto attr = lagrange::attribute_vector_ref<double>(mesh, "random_attribute");
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist(-1e-10, 1e-10);
    attr = attr.unaryExpr([&](double) { return dist(gen); });

    lagrange::IsolineOptions options;
    options.attribute_id = id;
    auto out = trim_by_isoline(mesh, options);

    CHECK(out.get_num_facets() > 0);
    auto out_vertices = vertex_view(out);
    for (auto i : lagrange::range(out_vertices.size())) {
        CHECK(!std::isnan(out_vertices.data()[i]));
        CHECK(out_vertices.data()[i] >= -delta);
        CHECK(out_vertices.data()[i] <= 1 + delta);
    }
}

TEST_CASE("trim_by_isoline ellipse", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;
    Index num_dims = lagrange::invalid<Index>();
    Index n = 19;
    Index m = 27;
    Scalar delta = 0.3f;
    Scalar a = 1.2f;
    Scalar b = 0.5f;
    std::vector<Scalar> isovalues = {0.025f, 0.035f, 0.05f, 0.075f, 0.1f, 0.2f};

    SECTION("2D")
    {
        num_dims = 2;
    }

    SECTION("3D")
    {
        num_dims = 3;
    }

    // Create the mesh, and perturb it a bit.
    auto mesh = create_grid(n, m, num_dims, delta);
    auto vertices = vertex_view(mesh);

    // Define a field
    auto field_id = mesh.create_attribute<double>(
        "random_attribute",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar);
    auto r2 = lagrange::attribute_vector_ref<double>(mesh, "random_attribute");
    for (auto i : lagrange::range(r2.size())) {
        const auto v = vertices.row(i).eval();
        const Scalar x = (v.x() - 0.5f);
        const Scalar y = (v.y() - 0.5f);
        const Scalar x2 = x * x;
        const Scalar y2 = y * y;
        r2[i] = a * x2 + b * y2;
    }

    for (auto i : lagrange::range(isovalues.size())) {
        // Find the contour
        lagrange::IsolineOptions options;
        options.isovalue = isovalues[i];
        options.attribute_id = field_id;
        auto out = trim_by_isoline(mesh, options);
        REQUIRE(compute_euler(out) == 1);
        REQUIRE(is_manifold(out));

        // Compute the perimeter
        auto out_vertices = vertex_view(out);
        out.initialize_edges();
        Scalar perimeter_computed = 0;
        for (auto e : lagrange::range(out.get_num_edges())) {
            if (out.is_boundary_edge(e)) {
                auto v = out.get_edge_vertices(e);
                perimeter_computed += (out_vertices.row(v[0]) - out_vertices.row(v[1])).norm();
            }
        }

        // Find the analytical value of the perimeter
        const Scalar ea = sqrt(options.isovalue / a);
        const Scalar eb = sqrt(options.isovalue / b);
        // https://www.mathsisfun.com/geometry/ellipse-perimeter.html
        const Scalar h = (ea - eb) * (ea - eb) / ((ea + eb) * (ea + eb));
        const Scalar perimeter_analytical = lagrange::safe_cast<Scalar>(
            lagrange::internal::pi * (ea + eb) * (1 + 3 * h / (10 + sqrt(4 - 3 * h))));

        lagrange::logger().debug(
            "analytical: {}, computed: {}",
            perimeter_analytical,
            perimeter_computed);

        // Check the values
        // Only if the ellipse is fully contained in the square mesh
        if (ea < 0.5 && eb < 0.5) {
            const float eps_rel = 1e-1f;
            REQUIRE_THAT(
                perimeter_computed,
                Catch::Matchers::WithinRel(perimeter_analytical, eps_rel));
        }

        lagrange::io::save_mesh("ellipse_" + std::to_string(i) + ".obj", out);
    }
}

// TODO: Test trim on a nonmanifold mesh + one with indexed attribute (e.g. blub)

TEST_CASE("trim_by_isoline indexed", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    auto uv_id = lagrange::find_matching_attribute(mesh, lagrange::AttributeUsage::UV);

    lagrange::IsolineOptions options;
    options.attribute_id = uv_id.value();
    options.isovalue = 0.6;
    options.channel_index = 1;
    auto trimmed = trim_by_isoline(mesh, options);

    REQUIRE(compute_components(trimmed) == 2);
    REQUIRE(compute_euler(trimmed) == 2);
}

TEST_CASE("trim_by_isoline nonmanifold edge", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
        "open/core/topology/nonmanifold_edge.obj");
    triangulate_polygonal_facets(mesh);
    // auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/topology/nonoriented_edge.obj");

    auto pos_id = mesh.attr_id_vertex_to_position();
    lagrange::IsolineOptions options;
    options.attribute_id = pos_id;
    options.isovalue = 0.0;
    options.channel_index = 2;
    auto trimmed_z = trim_by_isoline(mesh, options);

    REQUIRE(compute_components(trimmed_z) == 1);
    REQUIRE(compute_euler(trimmed_z) == 1);
    REQUIRE(!is_edge_manifold(trimmed_z));
    REQUIRE(trimmed_z.get_num_facets() == 6);
    REQUIRE(trimmed_z.get_num_vertices() == 11);

    auto uv_id = lagrange::find_matching_attribute(mesh, lagrange::AttributeUsage::UV);
    options.attribute_id = uv_id.value();
    options.isovalue = 0.5;
    options.channel_index = 1;
    auto trimmed_uv = trim_by_isoline(mesh, options);

    // Interestingly, the trimmed result has one "shared" vertex (the endpoint of the nonmanifold
    // edge), and one deduplicated vertex (where the isoline crosses the nonmanifold edge). This is
    // expected since trimming based on an indexed isoline doesn't "deduplicate" vertices. But at
    // some point we should preserve per-facet and indexed-attributes when doing the trimming to
    // make it easier to "separate" those after the fact if we wanted?
    REQUIRE(compute_components(trimmed_uv) == 2);
    REQUIRE(compute_euler(trimmed_uv) == 1);
    REQUIRE(is_edge_manifold(trimmed_uv));
    REQUIRE(!is_oriented(trimmed_uv));
    REQUIRE(trimmed_uv.get_num_facets() == 6);
    REQUIRE(trimmed_uv.get_num_vertices() == 12);
}

TEST_CASE("trim_by_isoline nonoriented edge", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
        "open/core/topology/nonoriented_edge.obj");
    triangulate_polygonal_facets(mesh);

    auto pos_id = mesh.attr_id_vertex_to_position();
    lagrange::IsolineOptions options;
    options.attribute_id = pos_id;
    options.isovalue = 0.0;
    options.channel_index = 2;
    auto trimmed_z = trim_by_isoline(mesh, options);

    REQUIRE(compute_components(trimmed_z) == 1);
    REQUIRE(compute_euler(trimmed_z) == 1);
    REQUIRE(is_edge_manifold(trimmed_z));
    REQUIRE(!is_oriented(trimmed_z));
    REQUIRE(trimmed_z.get_num_facets() == 4);
    REQUIRE(trimmed_z.get_num_vertices() == 8);

    auto uv_id = lagrange::find_matching_attribute(mesh, lagrange::AttributeUsage::UV);
    options.attribute_id = uv_id.value();
    options.isovalue = 0.5;
    options.channel_index = 1;
    auto trimmed_uv = trim_by_isoline(mesh, options);

    REQUIRE(compute_components(trimmed_uv) == 1);
    REQUIRE(compute_euler(trimmed_uv) == 1);
    REQUIRE(is_edge_manifold(trimmed_uv));
    REQUIRE(!is_oriented(trimmed_uv));
    REQUIRE(trimmed_uv.get_num_facets() == 4);
    REQUIRE(trimmed_uv.get_num_vertices() == 8);
}


TEST_CASE("trim_by_isoline color interpolation", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0.5, 1});

    mesh.add_triangle(0, 1, 2);

    mesh.create_attribute<uint8_t>(
        "color",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        3);
    auto colors = lagrange::attribute_matrix_ref<uint8_t>(mesh, "color");
    colors.row(0) << 255, 0, 0;
    colors.row(1) << 0, 255, 0;
    colors.row(2) << 0, 0, 255;

    lagrange::IsolineOptions options;
    options.attribute_id = mesh.attr_id_vertex_to_position();
    options.channel_index = 1;
    options.isovalue = 0.3;
    lagrange::SurfaceMesh<Scalar, Index> trimmed;
    tbb::task_arena arena(1);
    arena.execute([&] { trimmed = trim_by_isoline(mesh, options); });

    auto colors_trimmed = lagrange::attribute_matrix_view<uint8_t>(trimmed, "color");
    lagrange::RowMatrix<uint8_t> expected_colors(4, 3);
    // clang-format off
    expected_colors <<  255, 0, 0,
                        0, 255, 0,
                        178, 0, 76,
                        0, 178, 76;
    // clang-format on
    REQUIRE(colors_trimmed == expected_colors);
}

TEST_CASE("trim_by_isoline facet attribute", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    // Two triangles forming a unit square, each carrying a distinct per-facet value.
    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({1, 1});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    mesh.create_attribute<int32_t>(
        "facet_id",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar);
    auto facet_id = lagrange::attribute_vector_ref<int32_t>(mesh, "facet_id");
    facet_id(0) = 10;
    facet_id(1) = 20;

    // Trim by the y coordinate at 0.5. Both triangles are cut, so every sub-facet must inherit the
    // facet attribute of the triangle it was carved out of.
    lagrange::IsolineOptions options;
    options.attribute_id = mesh.attr_id_vertex_to_position();
    options.channel_index = 1;
    options.isovalue = 0.5;

    lagrange::SurfaceMesh<Scalar, Index> trimmed;
    tbb::task_arena arena(1);
    arena.execute([&] { trimmed = trim_by_isoline(mesh, options); });

    REQUIRE(trimmed.get_num_facets() == 2);
    auto trimmed_id = lagrange::attribute_vector_view<int32_t>(trimmed, "facet_id");
    CHECK(trimmed_id(0) == 10);
    CHECK(trimmed_id(1) == 20);
}

TEST_CASE("trim_by_isoline corner and indexed attribute", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    // A single triangle whose corners c0, c1, c2 are vertices v0, v1, v2.
    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);

    // Corner attribute (one scalar per corner).
    mesh.create_attribute<double>(
        "corner_value",
        lagrange::AttributeElement::Corner,
        lagrange::AttributeUsage::Scalar);
    auto corner_value = lagrange::attribute_vector_ref<double>(mesh, "corner_value");
    corner_value << 1, 2, 4;

    // Indexed attribute: distinct value per corner.
    auto idx_id = mesh.create_attribute<double>(
        "indexed_value",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::Scalar,
        1);
    {
        auto& iattr = mesh.ref_indexed_attribute<double>(idx_id);
        iattr.values().resize_elements(3);
        auto values = iattr.values().ref_all();
        values[0] = 10;
        values[1] = 20;
        values[2] = 40;
        auto indices = iattr.indices().ref_all();
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
    }

    // Trim by the y coordinate at 0.5. Vertices v0 and v1 survive (y == 0); the isoline crosses
    // edge c1->c2 and edge c2->c0, each at parameter t = 0.5. The result is a single quad with
    // corners [v0, v1, cross(c1,c2), cross(c2,c0)].
    lagrange::IsolineOptions options;
    options.attribute_id = mesh.attr_id_vertex_to_position();
    options.channel_index = 1;
    options.isovalue = 0.5;

    lagrange::SurfaceMesh<Scalar, Index> trimmed;
    tbb::task_arena arena(1);
    arena.execute([&] { trimmed = trim_by_isoline(mesh, options); });

    REQUIRE(trimmed.get_num_facets() == 1);
    REQUIRE(trimmed.get_num_corners() == 4);

    // Corner attribute: surviving corners keep their value, crossings are linearly interpolated.
    auto out_corner = lagrange::attribute_vector_view<double>(trimmed, "corner_value");
    CHECK(out_corner(0) == 1.0); // v0
    CHECK(out_corner(1) == 2.0); // v1
    CHECK(out_corner(2) == 3.0); // (2 + 4) / 2
    CHECK(out_corner(3) == 2.5); // (4 + 1) / 2

    // Indexed attribute: same interpolation rule, with surviving corners keeping their value index.
    const auto& out_iattr = trimmed.get_indexed_attribute<double>("indexed_value");
    auto out_values = out_iattr.values().get_all();
    auto out_indices = out_iattr.indices().get_all();
    auto value_at = [&](Index c) { return out_values[out_indices[c]]; };
    CHECK(value_at(0) == 10.0); // v0
    CHECK(value_at(1) == 20.0); // v1
    CHECK(value_at(2) == 30.0); // (20 + 40) / 2
    CHECK(value_at(3) == 25.0); // (40 + 10) / 2
}

TEST_CASE("trim_by_isoline keep_attributes flag", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);

    mesh.create_attribute<double>(
        "vertex_value",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar);
    mesh.create_attribute<double>(
        "facet_value",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar);
    mesh.create_attribute<double>(
        "corner_value",
        lagrange::AttributeElement::Corner,
        lagrange::AttributeUsage::Scalar);
    auto idx_id = mesh.create_attribute<double>(
        "indexed_value",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::Scalar,
        1);
    {
        auto& iattr = mesh.ref_indexed_attribute<double>(idx_id);
        iattr.values().resize_elements(3);
        auto indices = iattr.indices().ref_all();
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
    }

    lagrange::IsolineOptions options;
    options.attribute_id = mesh.attr_id_vertex_to_position();
    options.channel_index = 1;
    options.isovalue = 0.5;

    // By default, attributes of every element type are propagated.
    auto kept = trim_by_isoline(mesh, options);
    CHECK(kept.has_attribute("vertex_value"));
    CHECK(kept.has_attribute("facet_value"));
    CHECK(kept.has_attribute("corner_value"));
    CHECK(kept.has_attribute("indexed_value"));

    // Disabling the flag drops all user attributes while keeping the same geometry.
    options.keep_attributes = false;
    auto dropped = trim_by_isoline(mesh, options);
    CHECK_FALSE(dropped.has_attribute("vertex_value"));
    CHECK_FALSE(dropped.has_attribute("facet_value"));
    CHECK_FALSE(dropped.has_attribute("corner_value"));
    CHECK_FALSE(dropped.has_attribute("indexed_value"));

    CHECK(dropped.get_num_vertices() == kept.get_num_vertices());
    CHECK(dropped.get_num_facets() == kept.get_num_facets());
    CHECK(dropped.get_num_corners() == kept.get_num_corners());
}

TEST_CASE("insert_isoline basic", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);

    // Facet attribute to verify both sub-facets inherit the parent value.
    mesh.create_attribute<int32_t>(
        "facet_id",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar);
    lagrange::attribute_vector_ref<int32_t>(mesh, "facet_id")(0) = 7;

    lagrange::IsolineOptions options;
    options.attribute_id = mesh.attr_id_vertex_to_position();
    options.channel_index = 1;
    options.isovalue = 0.5;

    auto out = insert_isoline(mesh, options);

    // The crossed triangle is split into a triangle + a quad, both retained.
    REQUIRE(out.get_num_facets() == 2);
    REQUIRE(out.get_num_vertices() == 5); // 3 original + 2 isocrossings

    // Both sub-facets keep the parent facet attribute.
    auto out_id = lagrange::attribute_vector_view<int32_t>(out, "facet_id");
    CHECK(out_id(0) == 7);
    CHECK(out_id(1) == 7);

    // The isoline is inserted as a single interior (non-boundary) edge whose endpoints both lie on
    // the isovalue line (y == 0.5).
    out.initialize_edges();
    auto positions = vertex_view(out);
    Index num_interior = 0;
    for (auto e : lagrange::range(out.get_num_edges())) {
        if (!out.is_boundary_edge(e)) {
            ++num_interior;
            auto v = out.get_edge_vertices(e);
            CHECK(positions(v[0], 1) == Scalar(0.5));
            CHECK(positions(v[1], 1) == Scalar(0.5));
        }
    }
    CHECK(num_interior == 1);
}

TEST_CASE("insert_isoline keeps both sides", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = create_grid<Scalar, Index>(15, 11, 2, 0.2f);
    auto field_id = mesh.create_attribute<double>(
        "field",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar);
    auto field = lagrange::attribute_vector_ref<double>(mesh, "field");
    auto vertices = vertex_view(mesh);
    for (auto i : lagrange::range(field.size())) {
        field[i] = vertices(i, 0) - 0.5123; // isoline at x = 0.5123, off the grid lines
    }

    lagrange::IsolineOptions options;
    options.attribute_id = field_id;
    options.isovalue = 0.0;

    options.keep_below = true;
    auto below = trim_by_isoline(mesh, options);
    options.keep_below = false;
    auto above = trim_by_isoline(mesh, options);
    auto inserted = insert_isoline(mesh, options);

    // Insert keeps both sides: its facet count equals the two trimmed halves combined.
    CHECK(inserted.get_num_facets() == below.get_num_facets() + above.get_num_facets());

    // Every original vertex is retained, plus the isocrossing vertices.
    CHECK(inserted.get_num_vertices() > mesh.get_num_vertices());

    // The propagated field is continuous: the newly created vertices all lie on the isoline, so
    // their interpolated field value must equal the isovalue.
    REQUIRE(inserted.has_attribute("field"));
    auto out_field = lagrange::attribute_vector_view<double>(inserted, "field");
    for (auto i : lagrange::range(mesh.get_num_vertices(), inserted.get_num_vertices())) {
        CHECK_THAT(out_field[i], Catch::Matchers::WithinAbs(options.isovalue, 1e-6));
    }
}

TEST_CASE("extract_isoline basic", "[isoline]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/hemisphere.obj");
    triangulate_polygonal_facets(mesh);

    auto pos_id = mesh.attr_id_vertex_to_position();
    lagrange::IsolineOptions iso_options;
    iso_options.attribute_id = pos_id;
    iso_options.isovalue = 0.5;
    iso_options.channel_index = 1;

    auto extracted = extract_isoline(mesh, iso_options);
    lagrange::io::save_mesh("hemisphere_isoline.ply", extracted);
    lagrange::ComponentOptions component_options;
    component_options.connectivity_type = lagrange::ConnectivityType::Vertex;
    REQUIRE(compute_components(extracted, component_options) == 1);
    REQUIRE(extracted.get_vertex_per_facet() == 2);
    REQUIRE(extracted.get_num_facets() == 85);
    REQUIRE(extracted.get_num_vertices() == 85);
}
