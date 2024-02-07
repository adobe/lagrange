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
#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/warning.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <numeric>
#include <random>
#include <set>
#include <unordered_set>

namespace {

template <typename E>
constexpr E invalid_enum()
{
    using ValueType = typename std::underlying_type<E>::type;
    return static_cast<E>(std::numeric_limits<ValueType>::max());
}

// get_edge_vertices for invalid c

template <typename Scalar, typename Index>
void test_mesh_construction()
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    // Acceptable dimensions
    {
        LA_REQUIRE_THROWS([]() { MeshType mesh(Index(0)); }());
        for (Index dim = 1; dim < 9; ++dim) {
            MeshType mesh(dim);
        }
    }

    // Add vertices (3D)
    {
        MeshType mesh;
        REQUIRE(mesh.get_dimension() == 3);
        const Index dim = mesh.get_dimension();
        Index nv = mesh.get_num_vertices();

        Scalar p2d[] = {Scalar(9.1), Scalar(9.2)};
        Scalar p3d[] = {Scalar(0.1), Scalar(0.2), Scalar(0.3)};
        LA_REQUIRE_THROWS(mesh.add_vertex(p2d));
        LA_REQUIRE_THROWS(mesh.add_vertex({Scalar(9.1), Scalar(9.2)}));
        mesh.add_vertex(p3d);
        mesh.add_vertex({Scalar(0.1), Scalar(0.2), Scalar(0.3)});
        nv = mesh.get_num_vertices();
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0; i < nv; ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == Scalar(0.1));
            REQUIRE(p[1] == Scalar(0.2));
            REQUIRE(p[2] == Scalar(0.3));
        }

        mesh.add_vertices(5);
        lagrange::testing::check_mesh(mesh);
        for (Index i = nv; i < mesh.get_num_vertices(); ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == 0);
            REQUIRE(p[1] == 0);
            REQUIRE(p[2] == 0);
        }
        nv = mesh.get_num_vertices();

        std::vector<Scalar> buffer(4 * dim);
        std::iota(buffer.begin(), buffer.end(), Scalar(11));
        mesh.add_vertices(4, buffer);
        lagrange::testing::check_mesh(mesh);
        for (Index i = nv, j = 0; i < mesh.get_num_vertices(); ++i, ++j) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            for (Index d = 0; d < dim; ++d) {
                REQUIRE(p[d] == lagrange::safe_cast<Scalar>(11 + j * dim + d));
            }
        }
        nv = mesh.get_num_vertices();

        mesh.add_vertices(5, [dim](Index i, lagrange::span<Scalar> p) noexcept {
            REQUIRE((i >= 0 && i < 5));
            REQUIRE(p.size() == dim);
            p[0] = Scalar(1.1);
            p[1] = Scalar(1.2);
            p[2] = Scalar(1.3);
        });
        lagrange::testing::check_mesh(mesh);
        for (Index i = nv; i < mesh.get_num_vertices(); ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == Scalar(1.1));
            REQUIRE(p[1] == Scalar(1.2));
            REQUIRE(p[2] == Scalar(1.3));
        }
    }

    // Add vertices (2D)
    {
        MeshType mesh(2);
        REQUIRE(mesh.get_dimension() == 2);
        const Index dim = mesh.get_dimension();
        Index nv = mesh.get_num_vertices();

        Scalar p2d[] = {Scalar(9.1), Scalar(9.2)};
        Scalar p3d[] = {Scalar(0.1), Scalar(0.2), Scalar(0.3)};
        LA_REQUIRE_THROWS(mesh.add_vertex(p3d));
        LA_REQUIRE_THROWS(mesh.add_vertex({Scalar(0.1), Scalar(0.2), Scalar(0.3)}));
        mesh.add_vertex(p2d);
        mesh.add_vertex({Scalar(9.1), Scalar(9.2)});
        nv = mesh.get_num_vertices();
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0; i < nv; ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == Scalar(9.1));
            REQUIRE(p[1] == Scalar(9.2));
        }

        mesh.add_vertices(5);
        lagrange::testing::check_mesh(mesh);
        for (Index i = nv; i < mesh.get_num_vertices(); ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == 0);
            REQUIRE(p[1] == 0);
        }
        nv = mesh.get_num_vertices();

        mesh.add_vertices(5, [dim](Index i, lagrange::span<Scalar> p) noexcept {
            REQUIRE((i >= 0 && i < 5));
            REQUIRE(p.size() == dim);
            p[0] = Scalar(1.1);
            p[1] = Scalar(1.2);
        });
        lagrange::testing::check_mesh(mesh);
        for (Index i = nv; i < mesh.get_num_vertices(); ++i) {
            auto p = mesh.get_position(i);
            REQUIRE(p.size() == dim);
            REQUIRE(p[0] == Scalar(1.1));
            REQUIRE(p[1] == Scalar(1.2));
        }
    }

    // Add single facet at a time
    {
        MeshType mesh;
        mesh.add_vertices(10);

        mesh.add_triangle(0, 1, 2);
        REQUIRE(mesh.is_triangle_mesh());
        lagrange::testing::check_mesh(mesh);
        {
            auto f = mesh.get_facet_vertices(0);
            REQUIRE(f.size() == 3);
            REQUIRE(f[0] == 0);
            REQUIRE(f[1] == 1);
            REQUIRE(f[2] == 2);
            REQUIRE(mesh.get_facet_vertex(0, 0) == 0);
            REQUIRE(mesh.get_facet_vertex(0, 1) == 1);
            REQUIRE(mesh.get_facet_vertex(0, 2) == 2);
        }

        mesh.add_quad(0, 1, 2, 3);
        REQUIRE(mesh.is_hybrid());
        lagrange::testing::check_mesh(mesh);
        {
            auto f = mesh.get_facet_vertices(1);
            REQUIRE(f.size() == 4);
            REQUIRE(f[0] == 0);
            REQUIRE(f[1] == 1);
            REQUIRE(f[2] == 2);
            REQUIRE(f[3] == 3);
        }

        mesh.add_polygon(5);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_facets() == 3);
        {
            auto f = mesh.get_facet_vertices(2);
            REQUIRE(f.size() == 5);
            for (Index v : f) {
                REQUIRE(v == 0);
            }
        }

        const Index poly[5] = {1, 2, 3, 4, 5};
        mesh.add_polygon(poly);
        lagrange::testing::check_mesh(mesh);
        {
            auto f = mesh.get_facet_vertices(3);
            REQUIRE(f.size() == 5);
            for (Index k = 0; k < 5; ++k) {
                REQUIRE(f[k] == poly[k]);
            }
        }

        mesh.add_polygon(5, [](lagrange::span<Index> f) noexcept {
            for (Index i = 0; i < f.size(); ++i) {
                f[i] = i + 2;
            }
        });
        lagrange::testing::check_mesh(mesh);
        {
            auto f = mesh.get_facet_vertices(4);
            REQUIRE(f.size() == 5);
            for (Index k = 0; k < 5; ++k) {
                REQUIRE(f[k] == k + 2);
            }
        }
    }

    // Add multiple facets at once
    {
        MeshType mesh;
        mesh.add_vertices(10);

        mesh.add_triangles(3);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 3);
            for (Index v : f) {
                REQUIRE(v == 0);
            }
        }

        const Index tri[] = {0, 1, 2, 3, 1, 2, 4, 1, 2};
        mesh.add_triangles(3, tri);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 3, j = 0; i < mesh.get_num_facets(); ++i, ++j) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 3);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == tri[3 * j + k]);
            }
        }

        mesh.add_triangles(3, [](Index k, lagrange::span<Index> f) noexcept {
            REQUIRE((k >= 0 && k < 3));
            REQUIRE(f.size() == 3);
            for (Index i = 0; i < f.size(); ++i) {
                f[i] = i;
            }
        });
        lagrange::testing::check_mesh(mesh);
        for (Index i = 6; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 3);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == k);
            }
        }
    }

    {
        MeshType mesh;
        mesh.add_vertices(10);

        mesh.add_quads(3);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 4);
            for (Index v : f) {
                REQUIRE(v == 0);
            }
        }

        const Index quad[] = {0, 1, 2, 3, 4, 1, 2, 3};
        mesh.add_quads(2, quad);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 3, j = 0; i < mesh.get_num_facets(); ++i, ++j) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 4);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == quad[4 * j + k]);
            }
        }

        mesh.add_quads(4, [](Index k, lagrange::span<Index> f) noexcept {
            REQUIRE((k >= 0 && k < 4));
            REQUIRE(f.size() == 4);
            for (Index i = 0; i < f.size(); ++i) {
                f[i] = i;
            }
        });
        lagrange::testing::check_mesh(mesh);
        for (Index i = 5; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 4);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == k);
            }
        }
    }

    {
        MeshType mesh;
        mesh.add_vertices(10);

        mesh.add_polygons(3, 5);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 5);
            for (Index v : f) {
                REQUIRE(v == 0);
            }
        }

        const Index poly[] = {0, 1, 2, 3, 4, 5, 1, 2, 3, 4};
        mesh.add_polygons(2, 5, poly);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 3, j = 0; i < mesh.get_num_facets(); ++i, ++j) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 5);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == poly[5 * j + k]);
            }
        }

        mesh.add_polygons(2, 5, [](Index k, lagrange::span<Index> f) {
            REQUIRE((k >= 0 && k < 2));
            REQUIRE(f.size() == 5);
            for (Index i = 0; i < f.size(); ++i) {
                f[i] = i;
            }
        });
        lagrange::testing::check_mesh(mesh);
        for (Index i = 5; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == 5);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == k);
            }
        }
    }

    {
        MeshType mesh;
        mesh.add_vertices(10);

        const Index sizes[] = {3, 5};
        const Index indices[] = {0, 1, 3, 0, 1, 2, 3, 4};
        mesh.add_hybrid(sizes);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 0, o = 0; i < mesh.get_num_facets(); ++i) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == sizes[i]);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == 0);
            }
            o += sizes[i];
        }

        mesh.add_hybrid(sizes, indices);
        lagrange::testing::check_mesh(mesh);
        for (Index i = 2, j = 0, o = 0; i < mesh.get_num_facets(); ++i, ++j) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == sizes[j]);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == indices[o + k]);
            }
            o += sizes[j];
        }

        std::vector<bool> eval(3, false);
        mesh.add_hybrid(
            3,
            [&](Index f) {
                REQUIRE(eval[f] == false);
                eval[f] = true;
                return f + 3;
            },
            [](Index k, lagrange::span<Index> t) {
                REQUIRE((k >= 0 && k < 3));
                REQUIRE(t.size() == k + 3);
                for (Index i = 0; i < t.size(); ++i) {
                    t[i] = i;
                }
            });
        lagrange::testing::check_mesh(mesh);
        for (Index i = 4, j = 0; i < mesh.get_num_facets(); ++i, ++j) {
            auto f = mesh.get_facet_vertices(i);
            REQUIRE(f.size() == j + 3);
            for (Index k = 0; k < f.size(); ++k) {
                REQUIRE(f[k] == k);
            }
        }
    }
}

template <typename Scalar, typename Index>
void test_element_removal(bool with_edges)
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    // Make sure those different syntaxes do compile
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(20);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 20);
        mesh.remove_vertices({0, 1, 2});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 17);
        {
            Index v[] = {0, 1};
            mesh.remove_vertices(v);
        }
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 15);
        std::vector<Index> v2{0, 1};
        mesh.remove_vertices(v2);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 13);
        std::array<Index, 3> v3{{0, 1, 4}};
        mesh.remove_vertices(v3);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 10);
        mesh.remove_vertices([](Index v) noexcept { return (v == 0) || (v == 2); });
        REQUIRE(mesh.get_num_vertices() == 8);
        lagrange::testing::check_mesh(mesh);
    }

    // Simple removal test with hybrid storage
    {
        MeshType mesh;
        if (with_edges) {
            mesh.initialize_edges();
            REQUIRE(mesh.has_edges());
        }
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.is_regular());

        mesh.add_quad(0, 1, 2, 3);
        REQUIRE(mesh.is_hybrid());
        REQUIRE(mesh.get_num_facets() == 2);
        lagrange::testing::check_mesh(mesh);

        Index f[] = {1};
        mesh.remove_facets(f);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.is_hybrid());
        lagrange::testing::check_mesh(mesh);

        Index f2[] = {0};
        mesh.remove_facets(f2);
        REQUIRE(mesh.is_hybrid());
        lagrange::testing::check_mesh(mesh);

        mesh.add_triangle(0, 1, 2);
        REQUIRE(mesh.is_hybrid());
        lagrange::testing::check_mesh(mesh);
    }

    // Facet removal simple
    {
        MeshType mesh;
        if (with_edges) {
            mesh.initialize_edges();
            REQUIRE(mesh.has_edges());
        }
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_facets({1});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_facets() == 3);
    }

    // Removal with an overlapping (but correct) reindexing
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        const Index num_corners = 3 + 4 + 3 + 5;
        auto id =
            mesh.template create_attribute<double>("color", lagrange::AttributeElement::Corner);
        lagrange::testing::check_mesh(mesh);
        auto& attr = mesh.template ref_attribute<double>(id);
        std::iota(attr.ref_all().begin(), attr.ref_all().end(), double(123));
        REQUIRE(mesh.get_num_facets() == 4);
        mesh.remove_facets([](Index f) noexcept { return f == 1; });
        REQUIRE(mesh.get_num_facets() == 3);
        lagrange::testing::check_mesh(mesh);
        REQUIRE(attr.get_num_elements() == num_corners - 4);
        // Ensure corner attributes are properly shifted
        for (Index c = 3, i = 7; c < attr.get_num_elements(); ++c, ++i) {
            REQUIRE(attr.get(c, 0) == 123.0 + double(i));
        }
    }

    // Removal with an incorrect reindexing
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        LA_REQUIRE_THROWS(mesh.remove_vertices({1, 5, 2}));

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        LA_REQUIRE_THROWS(mesh.remove_vertices({1, 5, 100}));

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        LA_REQUIRE_THROWS(mesh.remove_facets({2, 1}));

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        LA_REQUIRE_THROWS(mesh.remove_facets({0, 1, 100}));
    }

    // Clear vertices/facets
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.clear_facets();
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_corners() == 0);
        REQUIRE(mesh.get_num_vertices() == 10);

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        mesh.clear_vertices();
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_corners() == 0);
        REQUIRE(mesh.get_num_vertices() == 0);
    }

    // Remove vertices: no removal
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        mesh.remove_vertices([](Index) noexcept { return false; });
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 10);

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        mesh.remove_vertices({});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 10);
    }

    // Remove facets: no removal
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_facets([](Index) noexcept { return false; });
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_facets() == 4);

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_facets({});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_facets() == 4);
    }

    // Remove vertices: remove dangling facets (hybrid)
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices([](Index v) noexcept { return v == 3; });
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 2);

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices({3});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 2);
    }

    // Remove vertices: remove dangling facets (regular)
    {
        MeshType mesh;
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(2, 3, 4);
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices([](Index v) noexcept { return v == 3; });
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 2);

        mesh = MeshType();
        if (with_edges) mesh.initialize_edges();
        mesh.add_vertices(10);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_quad(0, 1, 2, 3);
        lagrange::testing::check_mesh(mesh);
        mesh.add_triangle(0, 1, 2);
        lagrange::testing::check_mesh(mesh);
        mesh.add_polygon({0, 1, 2, 3, 4});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices({3});
        lagrange::testing::check_mesh(mesh);
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 2);
    }
}

template <typename Scalar, typename Index>
void test_mesh_storage()
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    /// Empty mesh
    {
        MeshType mesh;
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.is_triangle_mesh());
        REQUIRE(mesh.is_quad_mesh());
        REQUIRE(!mesh.is_hybrid());
        REQUIRE(mesh.get_vertex_per_facet() == 0);
    }

    // Add triangles
    {
        MeshType mesh;
        mesh.add_triangles(1);
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.is_triangle_mesh());
        REQUIRE(!mesh.is_quad_mesh());
        REQUIRE(!mesh.is_hybrid());
        REQUIRE(mesh.get_vertex_per_facet() == 3);
    }

    // Add quads
    {
        MeshType mesh;
        mesh.add_quads(1);
        REQUIRE(mesh.is_regular());
        REQUIRE(!mesh.is_triangle_mesh());
        REQUIRE(mesh.is_quad_mesh());
        REQUIRE(!mesh.is_hybrid());
        REQUIRE(mesh.get_vertex_per_facet() == 4);
    }

    // Add triangles and quads
    {
        MeshType mesh;
        REQUIRE(mesh.is_regular());
        mesh.add_vertices(4);
        REQUIRE(mesh.is_regular());
        mesh.add_triangles(1);
        REQUIRE(mesh.is_regular());
        mesh.add_quads(1);
        REQUIRE(mesh.is_hybrid());
        LA_REQUIRE_THROWS(mesh.get_vertex_per_facet());
    }

    // Create a regular mesh with add_hybrid
    {
        for (Index facet_size = 3; facet_size <= 5; ++facet_size) {
            const Index num_facets = 10;
            MeshType mesh;
            REQUIRE(mesh.is_regular());
            mesh.add_vertices(facet_size);
            REQUIRE(mesh.is_regular());
            mesh.add_hybrid(
                num_facets,
                [&facet_size](Index) noexcept { return facet_size; },
                [](Index, lagrange::span<Index> t) noexcept {
                    for (Index lv = 0; lv < t.size(); ++lv) {
                        t[lv] = lv;
                    }
                });
            for (Index f = 0; f < num_facets; ++f) {
                REQUIRE(mesh.get_facet_size(f) == facet_size);
            }
            lagrange::testing::check_mesh(mesh);
            REQUIRE(mesh.is_regular());
        }
    }

    // Create a hybrid mesh with add_hybrid
    {
        for (Index facet_size = 3; facet_size <= 5; ++facet_size) {
            const Index num_facets = 4;
            MeshType mesh;
            REQUIRE(mesh.is_regular());
            mesh.add_vertices(num_facets + facet_size);
            REQUIRE(mesh.is_regular());
            mesh.add_hybrid(
                num_facets,
                [&facet_size](Index f) noexcept { return facet_size + f; },
                [](Index, lagrange::span<Index> t) noexcept {
                    for (Index lv = 0; lv < t.size(); ++lv) {
                        t[lv] = lv;
                    }
                });
            for (Index f = 0; f < num_facets; ++f) {
                REQUIRE(mesh.get_facet_size(f) == f + facet_size);
            }
            lagrange::testing::check_mesh(mesh);
            REQUIRE(mesh.is_hybrid());
        }
    }
}

template <typename Scalar, typename Index>
void test_copy_move(bool with_edges)
{
    using namespace lagrange;
    using MeshType = SurfaceMesh<Scalar, Index>;

    // Move assignment operator to self
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        LA_IGNORE_SELF_MOVE_WARNING_BEGIN
        mesh = std::move(mesh);
        const void* new_addr = mesh.get_vertex_to_position().get_all().data();
        LA_IGNORE_SELF_MOVE_WARNING_END
        REQUIRE(old_addr == new_addr);

        if (with_edges) REQUIRE(mesh.has_edges());
    }

    // Move constructor to another variable
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        MeshType new_mesh(std::move(mesh));
        const void* new_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(old_addr == new_addr);

        // Write operation should not have created any copy
        new_mesh.ref_position(0)[0] = 1;
        const void* ref_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(ref_addr == old_addr);

        if (with_edges) REQUIRE(new_mesh.has_edges());
    }

    // Move assignment operator to another variable
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        MeshType new_mesh;
        new_mesh = std::move(mesh);
        const void* new_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(old_addr == new_addr);

        // Write operation should not have created any copy
        new_mesh.ref_position(0)[0] = 1;
        const void* ref_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(ref_addr == old_addr);

        if (with_edges) REQUIRE(new_mesh.has_edges());
    }

    // Copy assignment operator to self
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        LA_IGNORE_SELF_ASSIGN_WARNING_BEGIN
        mesh = mesh;
        LA_IGNORE_SELF_ASSIGN_WARNING_END
        const void* new_addr = mesh.get_vertex_to_position().get_all().data();
        REQUIRE(old_addr == new_addr);

        if (with_edges) REQUIRE(mesh.has_edges());
    }

    // Copy constructor to another variable
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        MeshType new_mesh(mesh);
        const void* new_addr = new_mesh.get_vertex_to_position().get_all().data();

        // Without write operation, address should be the same as before
        REQUIRE(old_addr == new_addr);

        // Write operation should create a copy
        new_mesh.ref_position(0)[0] = 1;
        const void* ref_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(ref_addr != old_addr);
        REQUIRE(mesh.get_vertex_to_position().get_all().data() == old_addr);

        if (with_edges) REQUIRE(new_mesh.has_edges());
    }

    // Copy assignment operator to another variable
    {
        MeshType mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        if (with_edges) mesh.initialize_edges();

        const void* old_addr = mesh.get_vertex_to_position().get_all().data();
        MeshType new_mesh;
        new_mesh = mesh;
        const void* new_addr = new_mesh.get_vertex_to_position().get_all().data();

        // Without write operation, address should be the same as before
        REQUIRE(old_addr == new_addr);

        // Write operation should create a copy
        new_mesh.ref_position(0)[0] = 1;
        const void* ref_addr = new_mesh.get_vertex_to_position().get_all().data();
        REQUIRE(ref_addr != old_addr);
        REQUIRE(mesh.get_vertex_to_position().get_all().data() == old_addr);

        if (with_edges) REQUIRE(new_mesh.has_edges());
    }

    ////////////////////////////////////////////////////////////////
    // Test that copy/move assignment destroys edges/connectivity //
    ////////////////////////////////////////////////////////////////

    // Copy assign
    if (with_edges) {
        MeshType mesh;
        mesh.add_vertices(5);
        mesh.add_triangles(2);

        MeshType new_mesh;
        new_mesh.add_vertices(10);
        new_mesh.add_triangles(3);
        new_mesh.initialize_edges();

        REQUIRE(!mesh.has_edges());
        REQUIRE(new_mesh.has_edges());
        new_mesh = mesh;
        REQUIRE(!mesh.has_edges());
        REQUIRE(!new_mesh.has_edges());
    }

    // Move assign
    if (with_edges) {
        MeshType mesh;
        mesh.add_vertices(5);
        mesh.add_triangles(2);

        MeshType new_mesh;
        new_mesh.add_vertices(10);
        new_mesh.add_triangles(3);
        new_mesh.initialize_edges();

        REQUIRE(!mesh.has_edges());
        REQUIRE(new_mesh.has_edges());
        new_mesh = std::move(mesh);
        REQUIRE(!new_mesh.has_edges());
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_normal_attribute()
{
    if constexpr (std::is_integral_v<ValueType>) {
        // Normal attributes have to be floating point
        return;
    }

    using namespace lagrange;

    for (size_t dim = 1; dim < 9u; ++dim) {
        SurfaceMesh<Scalar, Index> mesh(safe_cast<Index>(dim));
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(1);

        const size_t kmin = std::max<size_t>(1u, (dim > 0 ? dim - 1 : dim));
        const size_t kmax = dim + 1;
        for (size_t num_channels = kmin; num_channels <= kmax; ++num_channels) {
            if (num_channels == dim || num_channels == dim + 1) {
                REQUIRE_NOTHROW(mesh.template create_attribute<ValueType>(
                    fmt::format("normals_{}", num_channels),
                    AttributeElement::Vertex,
                    AttributeUsage::Normal,
                    num_channels));
            } else {
                LA_REQUIRE_THROWS(mesh.template create_attribute<ValueType>(
                    fmt::format("normals_{}", num_channels),
                    AttributeElement::Vertex,
                    AttributeUsage::Normal,
                    num_channels));
            }
        }
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_mesh_attribute()
{
    using namespace lagrange;

    const size_t num_channels = 3;
    const AttributeUsage usage = AttributeUsage::Color;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(5);
    mesh.add_quads(1);

    std::vector<ValueType> buffer(mesh.get_num_vertices() * mesh.get_dimension());
    std::iota(buffer.begin(), buffer.end(), safe_cast<ValueType>(11));

    std::vector<Index> indices(mesh.get_num_corners());
    std::iota(indices.begin(), indices.end(), Index(0));

    // Create attribute
    auto id_v0 = mesh.template create_attribute<ValueType>(
        "colors_v0",
        AttributeElement::Vertex,
        usage,
        num_channels,
        buffer);
    auto id_v1 = mesh.template create_attribute<ValueType>(
        "colors_v1",
        AttributeElement::Vertex,
        usage,
        num_channels);
    auto id_f = mesh.template create_attribute<ValueType>(
        "colors_f",
        AttributeElement::Facet,
        usage,
        num_channels);
    auto id_c = mesh.template create_attribute<ValueType>(
        "colors_c",
        AttributeElement::Corner,
        usage,
        num_channels);
    auto id_e = mesh.template create_attribute<ValueType>(
        "colors_e",
        AttributeElement::Edge,
        usage,
        num_channels);
    mesh.template create_attribute<ValueType>(
        "colors_i",
        AttributeElement::Indexed,
        usage,
        num_channels);
    mesh.template create_attribute<ValueType>(
        "colors_i_init",
        AttributeElement::Indexed,
        usage,
        num_channels,
        buffer,
        indices);
    LA_REQUIRE_THROWS(mesh.template create_attribute<ValueType>(
        "colors_v0",
        AttributeElement::Vertex,
        usage,
        num_channels));
    mesh.template create_attribute<ValueType>(
        "colors_x",
        AttributeElement::Value,
        usage,
        num_channels);
    REQUIRE(mesh.has_attribute("colors_e"));
    REQUIRE(mesh.has_attribute("colors_i"));
    REQUIRE(mesh.has_attribute("colors_x"));
    REQUIRE(mesh.template is_attribute_type<ValueType>("colors_v0"));
    lagrange::testing::check_mesh(mesh);

    auto attr_v0 = mesh.template ref_attribute<ValueType>("colors_v0").get_all();
    REQUIRE(attr_v0.size() == mesh.get_num_vertices() * mesh.get_dimension());
    for (size_t i = 0; i < attr_v0.size(); ++i) {
        REQUIRE(attr_v0[i] == buffer[i]);
        REQUIRE(attr_v0[i] == lagrange::safe_cast<ValueType>(11 + i));
    }

    auto attr_v1 = mesh.template ref_attribute<ValueType>("colors_v1").ref_all();
    REQUIRE(attr_v1.size() == mesh.get_num_vertices() * mesh.get_dimension());
    std::iota(attr_v1.begin(), attr_v1.end(), lagrange::safe_cast<ValueType>(23));

    SurfaceMesh<Scalar, Index> other;
    LA_REQUIRE_THROWS(other.create_attribute_from("colors_v0", mesh, "colors_v1"));
    other.add_vertices(10);
    other.create_attribute_from("colors_v0", mesh, "colors_v1");
    auto other_v0 = other.template get_attribute<ValueType>("colors_v0").get_all();
    REQUIRE(other_v0.size() == other.get_num_vertices() * other.get_dimension());
    for (size_t i = 0; i < other_v0.size(); ++i) {
        REQUIRE(other_v0[i] == attr_v1[i]);
        REQUIRE(other_v0[i] == safe_cast<ValueType>(23 + i));
    }

    // Get id
    REQUIRE(id_v0 == mesh.get_attribute_id("colors_v0"));
    REQUIRE(id_v1 == mesh.get_attribute_id("colors_v1"));
    REQUIRE(id_f == mesh.get_attribute_id("colors_f"));
    REQUIRE(id_c == mesh.get_attribute_id("colors_c"));
    REQUIRE(id_e == mesh.get_attribute_id("colors_e"));
    LA_REQUIRE_THROWS(mesh.get_attribute_id("bogus_name"));

    // Get name
    REQUIRE("colors_v0" == mesh.get_attribute_name(id_v0));
    REQUIRE("colors_v1" == mesh.get_attribute_name(id_v1));
    REQUIRE("colors_f" == mesh.get_attribute_name(id_f));
    REQUIRE("colors_c" == mesh.get_attribute_name(id_c));
    REQUIRE("colors_e" == mesh.get_attribute_name(id_e));
    LA_REQUIRE_THROWS(mesh.get_attribute_name(invalid_attribute_id()));

    // Delete attr
    LA_REQUIRE_THROWS(mesh.delete_attribute(mesh.attr_name_vertex_to_position()));
    LA_REQUIRE_THROWS(mesh.delete_attribute("bogus_name"));
    mesh.delete_attribute("colors_v1");
    REQUIRE(!mesh.has_attribute("colors_v1"));
    REQUIRE(id_v0 == mesh.get_attribute_id("colors_v0"));
    REQUIRE(id_f == mesh.get_attribute_id("colors_f"));
    REQUIRE(id_c == mesh.get_attribute_id("colors_c"));
    REQUIRE(id_e == mesh.get_attribute_id("colors_e"));

    // Create again (old id should be valid, new id should reuse delete id)
    auto new_id_v1 =
        mesh.template create_attribute<ValueType>("colors_v1", AttributeElement::Facet, usage, 3);
    REQUIRE(id_v0 == mesh.get_attribute_id("colors_v0"));
    REQUIRE(id_f == mesh.get_attribute_id("colors_f"));
    REQUIRE(id_c == mesh.get_attribute_id("colors_c"));
    REQUIRE(id_e == mesh.get_attribute_id("colors_e"));

    // Not strictly required by the API, but given the current implementation this should be true
    REQUIRE(id_v1 == new_id_v1);
    REQUIRE(id_v0 < id_v1);
    REQUIRE(id_v1 < id_f);
    REQUIRE(id_f < id_c);
    REQUIRE(id_c < id_e);

    // Rename attr
    LA_REQUIRE_THROWS(mesh.rename_attribute("bogus_name", "new_name"));
    LA_REQUIRE_THROWS(mesh.rename_attribute("colors_f", "colors_v0"));
    REQUIRE(mesh.has_attribute("colors_f"));
    REQUIRE(mesh.has_attribute("colors_v0"));
    mesh.rename_attribute("colors_f", "colors_f1");
    REQUIRE(!mesh.has_attribute("colors_f"));
    REQUIRE(mesh.has_attribute("colors_f1"));

    // Duplicate attr
    LA_REQUIRE_THROWS(mesh.duplicate_attribute("colors_v0", "colors_v1"));
    LA_REQUIRE_THROWS(mesh.duplicate_attribute("colors_v3", "colors_v4"));
    mesh.duplicate_attribute("colors_v0", "colors_v2");
    mesh.duplicate_attribute("colors_i", "colors_i2");
    mesh.duplicate_attribute("colors_x", "colors_x2");
    REQUIRE(
        mesh.template get_attribute<ValueType>("colors_v0").get_all().data() ==
        mesh.template get_attribute<ValueType>("colors_v2").get_all().data());

    // While this would leave the mesh in an unsafe state, it is possible to delete those attributes
    // too
    mesh = SurfaceMesh<Scalar, Index>();
    mesh.add_vertices(10);
    mesh.add_triangles(1);
    mesh.add_quads(1);
    REQUIRE_NOTHROW(
        mesh.delete_attribute(mesh.attr_name_vertex_to_position(), AttributeDeletePolicy::Force));
    REQUIRE_NOTHROW(
        mesh.delete_attribute(mesh.attr_name_corner_to_vertex(), AttributeDeletePolicy::Force));
    REQUIRE_NOTHROW(mesh.delete_attribute(
        mesh.attr_name_facet_to_first_corner(),
        AttributeDeletePolicy::Force));
}

template <typename ValueType, typename Scalar, typename Index>
void test_wrap_attribute()
{
    using namespace lagrange;

    const size_t num_channels = 3;
    const AttributeElement elem = AttributeElement::Vertex;
    const AttributeUsage usage = AttributeUsage::Color;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(2);
    mesh.add_quads(1);
    mesh.add_triangles(2);

    const size_t num_values = mesh.get_num_vertices() / 2;

    std::vector<ValueType> buffer(mesh.get_num_vertices() * num_channels);
    std::iota(buffer.begin(), buffer.end(), safe_cast<ValueType>(12));

    std::vector<Index> indices(mesh.get_num_corners());
    std::iota(indices.begin(), indices.end(), Index(0));

    // Wrap buffer as attribute
    {
        auto id =
            mesh.template wrap_as_attribute<ValueType>("colors", elem, usage, num_channels, buffer);
        auto ptr = mesh.template ref_attribute<ValueType>(id).ref_all();
        std::iota(ptr.begin(), ptr.end(), safe_cast<ValueType>(23));
        for (size_t i = 0; i < buffer.size(); ++i) {
            REQUIRE(buffer[i] == safe_cast<ValueType>(23 + i));
        }
        REQUIRE(ptr.data() == buffer.data());
    }

    // Wrap indexed attribute
    {
        auto id = mesh.template wrap_as_indexed_attribute<ValueType>(
            "indexed_colors",
            usage,
            num_values,
            num_channels,
            buffer,
            indices);
        auto& attr = mesh.template ref_indexed_attribute<ValueType>(id);
        std::iota(
            attr.values().ref_all().begin(),
            attr.values().ref_all().end(),
            safe_cast<ValueType>(23));
        for (size_t i = 0; i < num_values * num_channels; ++i) {
            REQUIRE(buffer[i] == safe_cast<ValueType>(23 + i));
        }
        REQUIRE(attr.values().get_all().data() == buffer.data());
        REQUIRE(attr.indices().get_all().data() == indices.data());
    }

    // Already exists
    LA_REQUIRE_THROWS(
        mesh.template wrap_as_attribute<ValueType>("colors", elem, usage, num_channels, buffer));

    // Wrap a smaller buffer
    std::vector<ValueType> buffer_small(buffer.size() / 2);
    LA_REQUIRE_THROWS(mesh.template wrap_as_attribute<ValueType>(
        "colors2",
        elem,
        usage,
        num_channels,
        buffer_small));
    REQUIRE(!mesh.has_attribute("colors2"));

    // Wrap a larger buffer
    std::vector<ValueType> buffer_large(buffer.size() * 2);
    REQUIRE_NOTHROW(mesh.template wrap_as_attribute<ValueType>(
        "colors2",
        elem,
        usage,
        num_channels,
        buffer_large));
    REQUIRE(mesh.has_attribute("colors2"));

    // Wrap as const attr
    {
        auto id = mesh.template wrap_as_const_attribute<ValueType>(
            "colors3",
            elem,
            usage,
            num_channels,
            buffer);
        auto& attr = mesh.template ref_attribute<ValueType>(id);
        LA_REQUIRE_THROWS(attr.ref_all());
        REQUIRE(attr.get_all().data() == buffer.data());
    }

    // Wrap as const indexed attr
    {
        auto id = mesh.template wrap_as_const_indexed_attribute<ValueType>(
            "indexed_colors3",
            usage,
            num_values,
            num_channels,
            buffer,
            indices);
        auto& attr = mesh.template ref_indexed_attribute<ValueType>(id);
        LA_REQUIRE_THROWS(attr.values().ref_all());
        LA_REQUIRE_THROWS(attr.indices().ref_all());
        REQUIRE(attr.values().get_all().data() == buffer.data());
        REQUIRE(attr.indices().get_all().data() == indices.data());
    }
}

template <typename Scalar, typename Index>
void test_wrap_attribute_special()
{
    using namespace lagrange;
    const Index num_vertices = 15;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_triangles(1);
    mesh.add_quads(1);

    // Wrap buffer as vertices
    std::vector<Scalar> points(num_vertices * mesh.get_dimension());
    std::iota(points.begin(), points.end(), Scalar(9));
    mesh.wrap_as_vertices(points, num_vertices);
    REQUIRE(mesh.get_vertex_to_position().get_all().data() == points.data());
    REQUIRE(mesh.get_num_vertices() == num_vertices);
    for (Index v = 0; v < num_vertices; ++v) {
        for (Index d = 0; d < mesh.get_dimension(); ++d) {
            REQUIRE(mesh.get_position(v)[d] == points[v * mesh.get_dimension() + d]);
        }
    }

    // Wrap buffer as const vertices
    std::iota(points.begin(), points.end(), Scalar(11));
    mesh.wrap_as_const_vertices(points, num_vertices);
    REQUIRE(mesh.get_vertex_to_position().get_all().data() == points.data());
    REQUIRE(mesh.get_num_vertices() == num_vertices);
    LA_REQUIRE_THROWS(mesh.ref_position(0));

    {
        // Wrap buffer as facets
        const Index nvpf = 3;
        const Index num_facets = 4;
        std::vector<Index> corner_to_vertex(num_facets * nvpf);
        std::iota(corner_to_vertex.begin(), corner_to_vertex.end(), Index(0));
        mesh.wrap_as_facets(corner_to_vertex, num_facets, nvpf);
        REQUIRE(mesh.get_corner_to_vertex().get_all().data() == corner_to_vertex.data());
        REQUIRE(mesh.get_num_facets() == num_facets);
        REQUIRE(mesh.is_triangle_mesh());
        lagrange::testing::check_mesh(mesh);

        // Wrap buffer as const facets
        mesh.ref_corner_to_vertex().create_internal_copy();
        mesh.add_quads(1);
        REQUIRE(mesh.is_hybrid());
        mesh.wrap_as_const_facets(corner_to_vertex, num_facets, nvpf);
        REQUIRE(mesh.get_num_facets() == num_facets);
        REQUIRE(mesh.is_triangle_mesh());
        LA_REQUIRE_THROWS(mesh.ref_facet_vertices(0));
        lagrange::testing::check_mesh(mesh);
    }

    {
        // Wrap buffer as facets + indices
        std::vector<Index> corner_to_vertex(3 + 4 + 5);
        std::vector<Index> offsets({0, 3, 3 + 4});
        const Index num_facets = static_cast<Index>(offsets.size());
        const Index num_corners = static_cast<Index>(corner_to_vertex.size());
        std::iota(corner_to_vertex.begin(), corner_to_vertex.end(), Index(0));
        mesh.wrap_as_facets(offsets, num_facets, corner_to_vertex, num_corners);
        REQUIRE(mesh.get_corner_to_vertex().get_all().data() == corner_to_vertex.data());
        REQUIRE(
            mesh.template get_attribute<Index>(mesh.attr_id_facet_to_first_corner())
                .get_all()
                .data() == offsets.data());
        REQUIRE(mesh.get_num_facets() == num_facets);
        REQUIRE(mesh.is_hybrid());
        lagrange::testing::check_mesh(mesh);

        // Wrap as const facets + indices
        mesh = SurfaceMesh<Scalar, Index>();
        mesh.add_vertices(num_vertices);
        mesh.wrap_as_const_facets(offsets, num_facets, corner_to_vertex, num_corners);
        REQUIRE(mesh.get_num_facets() == num_facets);
        REQUIRE(mesh.is_hybrid());
        LA_REQUIRE_THROWS(mesh.ref_facet_vertices(0));
        lagrange::testing::check_mesh(mesh);
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_export_attribute()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const AttributeElement elem = AttributeElement::Vertex;
    const AttributeUsage usage = AttributeUsage::Color;

    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(6);

        // Export regular attr
        {
            auto id =
                mesh.template create_attribute<ValueType>("colors", elem, usage, num_channels);
            auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
            REQUIRE(attr.size() == mesh.get_num_vertices() * mesh.get_dimension());
            std::iota(attr.begin(), attr.end(), ValueType(23));
            const void* old_ptr = attr.data();

            auto attr_ptr = mesh.template delete_and_export_attribute<ValueType>("colors");
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            REQUIRE(span.data() == old_ptr);
            for (size_t i = 0; i < span.size(); ++i) {
                REQUIRE(span[i] == safe_cast<ValueType>(i + 23));
            }
        }

        // Export regular attr (as const)
        {
            auto id =
                mesh.template create_attribute<ValueType>("colors", elem, usage, num_channels);
            auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
            REQUIRE(attr.size() == mesh.get_num_vertices() * mesh.get_dimension());
            std::iota(attr.begin(), attr.end(), ValueType(23));

            auto attr_ptr = mesh.template delete_and_export_const_attribute<ValueType>("colors");
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            for (size_t i = 0; i < span.size(); ++i) {
                REQUIRE(span[i] == safe_cast<ValueType>(i + 23));
            }
        }

        // Export indexed attr
        const size_t num_values = 13;
        {
            auto id = mesh.template create_attribute<ValueType>(
                "indexed_colors",
                AttributeElement::Indexed,
                usage,
                num_channels);
            auto& attr = mesh.template ref_indexed_attribute<ValueType>(id);
            attr.values().resize_elements(num_values);
            std::iota(
                attr.values().ref_all().begin(),
                attr.values().ref_all().end(),
                ValueType(23));
            const void* values_ptr = attr.values().get_all().data();
            const void* indices_ptr = attr.indices().get_all().data();

            auto attr_ptr =
                mesh.template delete_and_export_indexed_attribute<ValueType>("indexed_colors");
            REQUIRE(!mesh.has_attribute("indexed_colors"));
            auto values = attr_ptr->values().get_all();
            auto indices = attr_ptr->indices().get_all();
            REQUIRE(values.data() == values_ptr);
            REQUIRE(indices.data() == indices_ptr);
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == safe_cast<ValueType>(i + 23));
            }
        }

        // Export indexed attr (as const)
        {
            auto id = mesh.template create_attribute<ValueType>(
                "indexed_colors",
                AttributeElement::Indexed,
                usage,
                num_channels);
            auto& attr = mesh.template ref_indexed_attribute<ValueType>(id);
            attr.values().resize_elements(num_values);
            std::iota(
                attr.values().ref_all().begin(),
                attr.values().ref_all().end(),
                ValueType(23));
            const void* values_ptr = attr.values().get_all().data();
            const void* indices_ptr = attr.indices().get_all().data();

            auto attr_ptr = mesh.template delete_and_export_const_indexed_attribute<ValueType>(
                "indexed_colors");
            REQUIRE(!mesh.has_attribute("indexed_colors"));
            auto values = attr_ptr->values().get_all();
            auto indices = attr_ptr->indices().get_all();
            REQUIRE(values.data() == values_ptr);
            REQUIRE(indices.data() == indices_ptr);
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == safe_cast<ValueType>(i + 23));
            }
        }
    }

    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(5);
        mesh.add_quads(6);

        // Export external attr
        std::vector<ValueType> buffer(mesh.get_num_vertices() * num_channels);
        std::iota(buffer.begin(), buffer.end(), ValueType(12));

        // Copy if external (default)
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            auto attr_ptr = mesh.template delete_and_export_attribute<ValueType>("colors");
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            REQUIRE(span.data() != buffer.data());
            for (size_t i = 0; i < span.size(); ++i) {
                REQUIRE(span[i] == safe_cast<ValueType>(i + 12));
            }
        }

        // Keep external ptr
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            auto attr_ptr = mesh.template delete_and_export_attribute<ValueType>(
                "colors",
                AttributeDeletePolicy::ErrorIfReserved,
                AttributeExportPolicy::KeepExternalPtr);
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            REQUIRE(span.data() == buffer.data());
        }

        // Error if external
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            LA_REQUIRE_THROWS(mesh.template delete_and_export_attribute<ValueType>(
                "colors",
                AttributeDeletePolicy::ErrorIfReserved,
                AttributeExportPolicy::ErrorIfExternal));
            mesh.delete_attribute("colors");
        }

        // Copy if external (default)
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            auto attr_ptr = mesh.template delete_and_export_const_attribute<ValueType>("colors");
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            REQUIRE(span.data() != buffer.data());
            for (size_t i = 0; i < span.size(); ++i) {
                REQUIRE(span[i] == safe_cast<ValueType>(i + 12));
            }
        }

        // Keep external ptr
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            auto attr_ptr = mesh.template delete_and_export_const_attribute<ValueType>(
                "colors",
                AttributeDeletePolicy::ErrorIfReserved,
                AttributeExportPolicy::KeepExternalPtr);
            REQUIRE(!mesh.has_attribute("colors"));
            auto span = attr_ptr->get_all();
            REQUIRE(span.data() == buffer.data());
        }

        // Error if external
        {
            auto id = mesh.template wrap_as_attribute<ValueType>(
                "colors",
                elem,
                usage,
                num_channels,
                buffer);
            REQUIRE(mesh.template get_attribute<ValueType>(id).get_all().data() == buffer.data());
            LA_REQUIRE_THROWS(mesh.template delete_and_export_const_attribute<ValueType>(
                "colors",
                AttributeDeletePolicy::ErrorIfReserved,
                AttributeExportPolicy::ErrorIfExternal));
            mesh.delete_attribute("colors");
        }

        // Invalid export policy
        {
            mesh.template wrap_as_attribute<ValueType>("colors", elem, usage, num_channels, buffer);
            LA_REQUIRE_THROWS(mesh.template delete_and_export_const_attribute<ValueType>(
                "colors",
                AttributeDeletePolicy::ErrorIfReserved,
                invalid_enum<AttributeExportPolicy>()));
            mesh.delete_attribute("colors");
        }
    }
}

template <typename Scalar, typename Index>
void test_export_attribute_special()
{
    using namespace lagrange;

    // Export vertices/facets
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(1);
        mesh.add_quads(1);
        REQUIRE_NOTHROW(mesh.template delete_and_export_attribute<Scalar>(
            mesh.attr_name_vertex_to_position(),
            AttributeDeletePolicy::Force));
        REQUIRE_NOTHROW(mesh.template delete_and_export_attribute<Index>(
            mesh.attr_name_corner_to_vertex(),
            AttributeDeletePolicy::Force));
        REQUIRE_NOTHROW(mesh.template delete_and_export_attribute<Index>(
            mesh.attr_name_facet_to_first_corner(),
            AttributeDeletePolicy::Force));
    }

    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(1);
        mesh.add_quads(1);
        REQUIRE_NOTHROW(mesh.template delete_and_export_const_attribute<Scalar>(
            mesh.attr_name_vertex_to_position(),
            AttributeDeletePolicy::Force));
        REQUIRE_NOTHROW(mesh.template delete_and_export_const_attribute<Index>(
            mesh.attr_name_corner_to_vertex(),
            AttributeDeletePolicy::Force));
        REQUIRE_NOTHROW(mesh.template delete_and_export_const_attribute<Index>(
            mesh.attr_name_facet_to_first_corner(),
            AttributeDeletePolicy::Force));
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_indexed_attribute()
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(3);
    mesh.add_quads(2);
    mesh.add_triangles(2);

    mesh.template create_attribute<ValueType>("colors", AttributeElement::Vertex);
    mesh.template create_attribute<ValueType>("uv", AttributeElement::Indexed);
    mesh.template create_attribute<ValueType>("material_id", AttributeElement::Facet);
    mesh.template create_attribute<ValueType>("normals", AttributeElement::Corner);
    mesh.template create_attribute<ValueType>("custom", AttributeElement::Value);
    LA_REQUIRE_THROWS(
        mesh.template create_attribute<ValueType>("invalid", invalid_enum<AttributeElement>()));
    REQUIRE(!mesh.is_attribute_indexed("colors"));
    REQUIRE(mesh.is_attribute_indexed("uv"));
    REQUIRE(!mesh.is_attribute_indexed("material_id"));
    REQUIRE(!mesh.is_attribute_indexed("normals"));
    REQUIRE(!mesh.is_attribute_indexed("custom"));
    REQUIRE(!mesh.has_attribute("invalid"));

#if LAGRANGE_TARGET_FEATURE(RTTI)
    {
        auto& attr_uv = mesh.get_attribute_base("uv");
        auto& attr_normals = mesh.get_attribute_base("normals");
        REQUIRE(dynamic_cast<const Attribute<ValueType>*>(&attr_uv) == nullptr);
        REQUIRE(dynamic_cast<const Attribute<ValueType>*>(&attr_normals));
        REQUIRE(dynamic_cast<const IndexedAttribute<ValueType, Index>*>(&attr_uv));
        REQUIRE(dynamic_cast<const IndexedAttribute<ValueType, Index>*>(&attr_normals) == nullptr);
    }
#endif

    {
        auto& attr_uv = mesh.template ref_indexed_attribute<ValueType>("uv");
        attr_uv.values().resize_elements(10);
        mesh.add_vertices(4);
        mesh.add_quads(3);
        REQUIRE(
            mesh.template get_indexed_attribute<ValueType>("uv").indices().get_all().data() ==
            attr_uv.indices().get_all().data());
        REQUIRE(attr_uv.values().get_num_elements() == 10);
    }
}


template <typename Scalar, typename Index>
void test_resize_attribute_basic()
{
    using namespace lagrange;

    {
        // Regular mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.template create_attribute<Scalar>("vertex", AttributeElement::Vertex);
        mesh.template create_attribute<Scalar>("facet", AttributeElement::Facet);
        mesh.template create_attribute<Scalar>("corner", AttributeElement::Corner);
        mesh.template create_attribute<Scalar>("value", AttributeElement::Value);
        mesh.template create_attribute<Scalar>("indexed", AttributeElement::Indexed);
        lagrange::testing::check_mesh(mesh);

        const Index num_vertices = 10;
        const Index num_facets = 6;
        const Index num_corners = num_facets * 3;
        mesh.add_vertices(num_vertices);
        mesh.add_triangles(6);
        REQUIRE(mesh.template get_attribute<Scalar>("vertex").get_num_elements() == num_vertices);
        REQUIRE(mesh.template get_attribute<Scalar>("facet").get_num_elements() == num_facets);
        REQUIRE(mesh.template get_attribute<Scalar>("corner").get_num_elements() == num_corners);
        REQUIRE(mesh.template get_attribute<Scalar>("value").get_num_elements() == 0);
        REQUIRE(
            mesh.template get_indexed_attribute<Scalar>("indexed").values().get_num_elements() ==
            0);
        REQUIRE(
            mesh.template get_indexed_attribute<Scalar>("indexed").indices().get_num_elements() ==
            num_corners);
        lagrange::testing::check_mesh(mesh);
    }

    {
        // Hybrid mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.template create_attribute<Scalar>("vertex", AttributeElement::Vertex);
        mesh.template create_attribute<Scalar>("facet", AttributeElement::Facet);
        mesh.template create_attribute<Scalar>("corner", AttributeElement::Corner);
        mesh.template create_attribute<Scalar>("value", AttributeElement::Value);
        mesh.template create_attribute<Scalar>("indexed", AttributeElement::Indexed);

        const Index num_vertices = 10;
        const Index num_facets = 6 + 2;
        const Index num_corners = 6 * 3 + 2 * 4;
        mesh.add_vertices(num_vertices);
        mesh.add_triangles(3);
        mesh.add_quads(2);
        mesh.add_triangles(3);
        REQUIRE(mesh.template get_attribute<Scalar>("vertex").get_num_elements() == num_vertices);
        REQUIRE(mesh.template get_attribute<Scalar>("facet").get_num_elements() == num_facets);
        REQUIRE(mesh.template get_attribute<Scalar>("corner").get_num_elements() == num_corners);
        REQUIRE(mesh.template get_attribute<Scalar>("value").get_num_elements() == 0);
        REQUIRE(
            mesh.template get_indexed_attribute<Scalar>("indexed").values().get_num_elements() ==
            0);
        REQUIRE(
            mesh.template get_indexed_attribute<Scalar>("indexed").indices().get_num_elements() ==
            num_corners);
        lagrange::testing::check_mesh(mesh);
    }

    // TODO: Wrap + resize other attr
}

template <typename Scalar, typename Index>
void test_edit_facets_with_edges()
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(6);
    mesh.initialize_edges();

    {
        // Throwing methods (no initial value provided by user)
        LA_REQUIRE_THROWS(mesh.ref_facet_vertices(0));
        LA_REQUIRE_THROWS(mesh.ref_corner_to_vertex());
        LA_REQUIRE_THROWS(mesh.add_triangles(1));
        LA_REQUIRE_THROWS(mesh.add_quads(1));
        LA_REQUIRE_THROWS(mesh.add_polygons(1, 5));
        const Index sizes[] = {3, 5};
        LA_REQUIRE_THROWS(mesh.add_hybrid(sizes));

        // Write access to facet indices via low-level method is still allowed
        REQUIRE_NOTHROW(mesh.template ref_attribute<Index>(mesh.attr_id_corner_to_vertex()));
    }
    {
        // Add triangle
        mesh.add_triangle(0, 1, 2);
        const Index indices[] = {0, 1, 2, 0, 1, 2};
        mesh.add_triangles(2, indices);
        mesh.add_triangles(3, [](Index, span<Index> t) {
            std::fill(t.begin(), t.end(), Index(0));
        });
        lagrange::testing::check_mesh(mesh);
    }
    {
        // Add quad
        mesh.add_quad(0, 1, 2, 3);
        const Index indices[] = {0, 1, 2, 3, 0, 1, 2, 3};
        mesh.add_quads(2, indices);
        mesh.add_quads(3, [](Index, span<Index> t) { std::fill(t.begin(), t.end(), Index(0)); });
        lagrange::testing::check_mesh(mesh);
    }
    {
        // Add polygon
        const Index facet[] = {0, 1, 2, 3, 4};
        mesh.add_polygon(facet);
        mesh.add_polygon({0, 1, 2, 3, 4});
        const Index indices[] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4};
        mesh.add_polygons(2, 5, indices);
        mesh.add_polygons(3, 5, [](Index, span<Index> t) {
            std::fill(t.begin(), t.end(), Index(0));
        });
        lagrange::testing::check_mesh(mesh);
    }
    {
        // Add hybrid
        const Index sizes[] = {3, 5};
        const Index indices[] = {0, 1, 3, 0, 1, 2, 3, 4};
        mesh.add_hybrid(sizes, indices);
        mesh.add_hybrid(
            3,
            [](Index f) noexcept { return f + 3; },
            [](Index, lagrange::span<Index> t) noexcept {
                std::fill(t.begin(), t.end(), Index(0));
            });
        lagrange::testing::check_mesh(mesh);
    }
}

template <typename Scalar, typename Index>
void test_user_edges()
{
    using namespace lagrange;

    auto is_same_edge = [](auto a, auto b) {
        return (std::min(a[0], a[1]) == std::min(b[0], b[1])) &&
               (std::max(a[0], a[1]) == std::max(b[0], b[1]));
    };

    auto is_same_edges = [&](const auto& mesh, const auto& edges) {
        if (mesh.get_num_edges() != safe_cast<Index>(edges.size())) {
            return false;
        }
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            if (!is_same_edge(edges[e], mesh.get_edge_vertices(e))) {
                return false;
            }
        }
        return true;
    };

    // Valid user ordering
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(1, 3, 4, 2);
        mesh.add_polygon({0, 2, 4, 5, 6});

        const std::vector<std::array<Index, 2>> edges = {{
            {{0, 1}},
            {{1, 3}},
            {{3, 4}},
            {{4, 5}},
            {{5, 6}},
            {{6, 0}},
            {{1, 2}},
            {{2, 0}},
            {{2, 4}},
        }};
        mesh.initialize_edges({&edges[0][0], 2 * edges.size()});
        REQUIRE(is_same_edges(mesh, edges));

        const auto shuffled_edges = [&]() {
            std::vector<std::array<Index, 2>> copy(edges);
            std::mt19937 gen;
            std::shuffle(copy.begin(), copy.end(), gen);
            return copy;
        }();

        // Re-initializing doesn't change the order -- we need to clear the mesh first
        mesh.initialize_edges({&shuffled_edges[0][0], 2 * shuffled_edges.size()});
        REQUIRE(is_same_edges(mesh, edges));
        REQUIRE(!is_same_edges(mesh, shuffled_edges));

        // If we clear the edge information, we can update with our new ordering
        mesh.clear_edges();
        mesh.initialize_edges({&shuffled_edges[0][0], 2 * shuffled_edges.size()});
        REQUIRE(!is_same_edges(mesh, edges));
        REQUIRE(is_same_edges(mesh, shuffled_edges));
    }

    // Ordering with missing edges
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(1, 3, 4, 2);
        mesh.add_polygon({0, 2, 4, 5, 6});

        const std::vector<std::array<Index, 2>> edges = {{
            {{0, 1}},
            {{1, 3}},
            {{4, 5}},
            {{5, 6}},
            {{1, 2}},
            {{2, 0}},
            {{2, 4}},
        }};
        LA_REQUIRE_THROWS(mesh.initialize_edges({&edges[0][0], 2 * edges.size()}));
    }

    // Ordering with invalid endpoints
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(1, 3, 4, 2);
        mesh.add_polygon({0, 2, 4, 5, 6});

        const std::vector<std::array<Index, 2>> edges = {{
            {{0, 1}},
            {{1, 3}},
            {{3, 4}},
            {{4, 5}},
            {{5, 5}},
            {{6, 0}},
            {{1, 2}},
            {{2, 0}},
            {{2, 4}},
        }};
        LA_REQUIRE_THROWS(mesh.initialize_edges({&edges[0][0], 2 * edges.size()}));
    }

    // Ordering with repeated edges
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(1, 3, 4, 2);
        mesh.add_polygon({0, 2, 4, 5, 6});

        const std::vector<std::array<Index, 2>> edges = {{
            {{0, 1}},
            {{1, 3}},
            {{3, 4}},
            {{4, 5}},
            {{4, 5}},
            {{5, 6}},
            {{6, 0}},
            {{1, 2}},
            {{2, 0}},
            {{2, 4}},
        }};
        LA_REQUIRE_THROWS(mesh.initialize_edges({&edges[0][0], 2 * edges.size()}));
    }

    // Ordering with repeated edges + missing edges
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(1, 3, 4, 2);
        mesh.add_polygon({0, 2, 4, 5, 6});

        const std::vector<std::array<Index, 2>> edges = {{
            {{0, 1}},
            {{1, 3}},
            {{3, 4}},
            {{4, 5}},
            {{4, 5}},
            {{5, 6}},
            {{6, 0}},
            {{2, 0}},
            {{2, 4}},
        }};
        LA_REQUIRE_THROWS(mesh.initialize_edges({&edges[0][0], 2 * edges.size()}));
    }
}

template <typename Scalar, typename Index>
void test_reserved_attribute_basic()
{
    using namespace lagrange;

    {
        // Regular mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_position()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_vertex()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_facet_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_corner_to_facet()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_corner_to_edge()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_edge_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_next_corner_around_edge()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_vertex_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_next_corner_around_vertex()));
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_position()) ==
            mesh.attr_id_vertex_to_positions());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_vertex()) ==
            mesh.attr_id_corner_to_vertex());
    }

    {
        // Hybrid mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.add_quads(3);
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_position()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_vertex()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_facet_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_facet()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_corner_to_edge()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_edge_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_next_corner_around_edge()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_vertex_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_next_corner_around_vertex()));
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_position()) ==
            mesh.attr_id_vertex_to_positions());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_vertex()) ==
            mesh.attr_id_corner_to_vertex());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_facet_to_first_corner()) ==
            mesh.attr_id_facet_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_facet()) ==
            mesh.attr_id_corner_to_facet());
    }

    {
        // Regular mesh with edges
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.initialize_edges();
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_position()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_vertex()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_facet_to_first_corner()));
        REQUIRE(!mesh.has_attribute(mesh.attr_name_corner_to_facet()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_edge()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_edge_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_next_corner_around_edge()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_next_corner_around_vertex()));
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_position()) ==
            mesh.attr_id_vertex_to_positions());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_vertex()) ==
            mesh.attr_id_corner_to_vertex());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_edge()) ==
            mesh.attr_id_corner_to_edge());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_edge_to_first_corner()) ==
            mesh.attr_id_edge_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_next_corner_around_edge()) ==
            mesh.attr_id_next_corner_around_edge());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_first_corner()) ==
            mesh.attr_id_vertex_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_next_corner_around_vertex()) ==
            mesh.attr_id_next_corner_around_vertex());
    }

    {
        // Hybrid mesh with edges
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.add_quads(3);
        mesh.initialize_edges();
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_position()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_vertex()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_facet_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_facet()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_corner_to_edge()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_edge_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_next_corner_around_edge()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_vertex_to_first_corner()));
        REQUIRE(mesh.has_attribute(mesh.attr_name_next_corner_around_vertex()));
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_position()) ==
            mesh.attr_id_vertex_to_positions());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_vertex()) ==
            mesh.attr_id_corner_to_vertex());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_facet_to_first_corner()) ==
            mesh.attr_id_facet_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_facet()) ==
            mesh.attr_id_corner_to_facet());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_corner_to_edge()) ==
            mesh.attr_id_corner_to_edge());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_edge_to_first_corner()) ==
            mesh.attr_id_edge_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_next_corner_around_edge()) ==
            mesh.attr_id_next_corner_around_edge());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_vertex_to_first_corner()) ==
            mesh.attr_id_vertex_to_first_corner());
        REQUIRE(
            mesh.get_attribute_id(mesh.attr_name_next_corner_around_vertex()) ==
            mesh.attr_id_next_corner_around_vertex());
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_custom_reserved_attributes()
{
    using namespace lagrange;

    {
        // Ensure that we cannot create reserved attributes accidentally
        const size_t num_channels = 3;
        const AttributeElement elem = AttributeElement::Vertex;
        const AttributeUsage usage = AttributeUsage::Normal;

        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);
        mesh.add_quads(2);
        mesh.add_triangles(2);

        const size_t num_values = mesh.get_num_vertices() / 2;

        std::vector<ValueType> buffer(mesh.get_num_vertices() * num_channels);
        std::iota(buffer.begin(), buffer.end(), safe_cast<ValueType>(12));

        std::vector<Index> indices(mesh.get_num_corners());
        std::iota(indices.begin(), indices.end(), Index(0));

        mesh.template create_attribute<ValueType>("colors", elem);

        LA_REQUIRE_THROWS(mesh.template create_attribute<ValueType>("$colors", elem));
        LA_REQUIRE_THROWS(mesh.template wrap_as_attribute<ValueType>(
            "$normals",
            elem,
            usage,
            num_channels,
            buffer));
        LA_REQUIRE_THROWS(mesh.template wrap_as_indexed_attribute<ValueType>(
            "$indexed_normals",
            usage,
            num_values,
            num_channels,
            buffer,
            indices));
        LA_REQUIRE_THROWS(mesh.template wrap_as_const_attribute<ValueType>(
            "$normals",
            elem,
            usage,
            num_channels,
            buffer));
        LA_REQUIRE_THROWS(mesh.template wrap_as_const_indexed_attribute<ValueType>(
            "$indexed_normals",
            usage,
            num_values,
            num_channels,
            buffer,
            indices));
        LA_REQUIRE_THROWS(mesh.create_attribute_from("colors", mesh, "$colors"));
        LA_REQUIRE_THROWS(mesh.duplicate_attribute("colors", "$colors"));
        LA_REQUIRE_THROWS(mesh.rename_attribute("colors", "$colors"));
    }

    {
        // Test create/deletion of reserved attributes with different types than default
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangles(3);

        auto create_reserved = [&](auto name) {
            mesh.template create_attribute<ValueType>(
                name,
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                1,
                {},
                {},
                AttributeCreatePolicy::Force);
        };

        mesh.delete_attribute(mesh.attr_name_vertex_to_position(), AttributeDeletePolicy::Force);
        mesh.delete_attribute(mesh.attr_name_corner_to_vertex(), AttributeDeletePolicy::Force);

        create_reserved(mesh.attr_name_vertex_to_position());
        create_reserved(mesh.attr_name_corner_to_vertex());
        create_reserved(mesh.attr_name_corner_to_facet());
        create_reserved(mesh.attr_name_corner_to_edge());
        create_reserved(mesh.attr_name_edge_to_first_corner());
        create_reserved(mesh.attr_name_next_corner_around_edge());
        create_reserved(mesh.attr_name_vertex_to_first_corner());
        create_reserved(mesh.attr_name_next_corner_around_vertex());

        // Use an invalid reserved attribute name
        std::string_view name = "$isjdlogjioewj";
        LA_REQUIRE_THROWS(create_reserved(name));
        LA_REQUIRE_THROWS(mesh.delete_attribute(name, AttributeDeletePolicy::Force));
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_element_index_type()
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(3);
    mesh.add_quads(2);
    mesh.add_triangles(2);
    mesh.initialize_edges();

    const AttributeElement elem = AttributeElement::Vertex;
    const size_t num_channels = 1;
    const size_t num_values = mesh.get_num_vertices() / 2;

    std::vector<ValueType> buffer(mesh.get_num_vertices() * num_channels);
    std::iota(buffer.begin(), buffer.end(), safe_cast<ValueType>(12));

    std::vector<Index> indices(mesh.get_num_corners());
    std::iota(indices.begin(), indices.end(), Index(0));

    auto usages = {
        AttributeUsage::VertexIndex,
        AttributeUsage::FacetIndex,
        AttributeUsage::CornerIndex,
        AttributeUsage::EdgeIndex,
    };

    int cnt = 0;
    auto get_name = [&cnt]() { return fmt::format("id_{}", cnt++); };
    for (auto usage : usages) {
        if constexpr (std::is_same_v<Index, ValueType>) {
            REQUIRE_NOTHROW(mesh.template create_attribute<ValueType>(get_name(), elem, usage));
            REQUIRE_NOTHROW(mesh.template wrap_as_attribute<ValueType>(
                get_name(),
                elem,
                usage,
                num_channels,
                buffer));
            REQUIRE_NOTHROW(mesh.template wrap_as_const_attribute<ValueType>(
                get_name(),
                elem,
                usage,
                num_channels,
                buffer));
            REQUIRE_NOTHROW(mesh.template wrap_as_indexed_attribute<ValueType>(
                get_name(),
                usage,
                num_values,
                num_channels,
                buffer,
                indices));
            REQUIRE_NOTHROW(mesh.template wrap_as_const_indexed_attribute<ValueType>(
                get_name(),
                usage,
                num_values,
                num_channels,
                buffer,
                indices));
        } else {
            LA_REQUIRE_THROWS(mesh.template create_attribute<ValueType>(get_name(), elem, usage));
            LA_REQUIRE_THROWS(mesh.template wrap_as_attribute<ValueType>(
                get_name(),
                elem,
                usage,
                num_channels,
                buffer));
            LA_REQUIRE_THROWS(mesh.template wrap_as_const_attribute<ValueType>(
                get_name(),
                elem,
                usage,
                num_channels,
                buffer));
            LA_REQUIRE_THROWS(mesh.template wrap_as_indexed_attribute<ValueType>(
                get_name(),
                usage,
                num_values,
                num_channels,
                buffer,
                indices));
            LA_REQUIRE_THROWS(mesh.template wrap_as_const_indexed_attribute<ValueType>(
                get_name(),
                usage,
                num_values,
                num_channels,
                buffer,
                indices));
        }
    }
}

template <typename Scalar, typename Index>
void test_element_index_resize()
{
    using namespace lagrange;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangle(0, 1, 2);
    mesh.add_quad(2, 3, 4, 5);
    mesh.add_quad(2, 3, 4, 5);
    mesh.add_triangle(5, 6, 7);
    mesh.add_triangle(6, 7, 8);
    mesh.initialize_edges();
    lagrange::testing::check_mesh(mesh);

    auto vid = mesh.template create_attribute<Index>(
        "vid",
        AttributeElement::Vertex,
        AttributeUsage::VertexIndex);
    auto fid = mesh.template create_attribute<Index>(
        "fid",
        AttributeElement::Facet,
        AttributeUsage::FacetIndex);
    auto cid = mesh.template create_attribute<Index>(
        "cid",
        AttributeElement::Corner,
        AttributeUsage::CornerIndex);
    auto eid = mesh.template create_attribute<Index>(
        "eid",
        AttributeElement::Edge,
        AttributeUsage::EdgeIndex);
    auto vid_i = mesh.template create_attribute<Index>(
        "vid_i",
        AttributeElement::Indexed,
        AttributeUsage::VertexIndex);
    auto fid_i = mesh.template create_attribute<Index>(
        "fid_i",
        AttributeElement::Indexed,
        AttributeUsage::FacetIndex);
    auto cid_i = mesh.template create_attribute<Index>(
        "cid_i",
        AttributeElement::Indexed,
        AttributeUsage::CornerIndex);
    auto eid_i = mesh.template create_attribute<Index>(
        "eid_i",
        AttributeElement::Indexed,
        AttributeUsage::EdgeIndex);
    lagrange::testing::check_mesh(mesh);

    // Initialize attribute values
    {
        auto vattr = mesh.template ref_attribute<Index>(vid).ref_all();
        std::iota(vattr.begin(), vattr.end(), Index(0));
        auto fattr = mesh.template ref_attribute<Index>(fid).ref_all();
        std::iota(fattr.begin(), fattr.end(), Index(0));
        auto cattr = mesh.template ref_attribute<Index>(cid).ref_all();
        std::iota(cattr.begin(), cattr.end(), Index(0));
        auto eattr = mesh.template ref_attribute<Index>(eid).ref_all();
        std::iota(eattr.begin(), eattr.end(), Index(0));

        auto vattr_i = mesh.template ref_indexed_attribute<Index>(vid_i).values().ref_all();
        std::iota(vattr_i.begin(), vattr_i.end(), Index(0));
        auto fattr_i = mesh.template ref_indexed_attribute<Index>(fid_i).values().ref_all();
        std::iota(fattr_i.begin(), fattr_i.end(), Index(0));
        auto cattr_i = mesh.template ref_indexed_attribute<Index>(cid_i).values().ref_all();
        std::iota(cattr_i.begin(), cattr_i.end(), Index(0));
        auto eattr_i = mesh.template ref_indexed_attribute<Index>(eid_i).values().ref_all();
        std::iota(eattr_i.begin(), eattr_i.end(), Index(0));
    }
    lagrange::testing::check_mesh(mesh);

    // Resize attr
    mesh.remove_facets({1});
    lagrange::testing::check_mesh(mesh);
    mesh.remove_vertices({5});
    lagrange::testing::check_mesh(mesh);

    // Clear mesh
    mesh.clear_facets();
    lagrange::testing::check_mesh(mesh);
    mesh.clear_vertices();
    lagrange::testing::check_mesh(mesh);
    mesh.clear_edges();
    lagrange::testing::check_mesh(mesh);
}

template <typename ValueType, typename Scalar, typename Index>
void test_resize_attribute_type()
{
    using namespace lagrange;

    auto check_attr = [](const SurfaceMesh<Scalar, Index>& mesh,
                         AttributeId id,
                         const std::vector<ValueType>& gt) {
        auto attr = mesh.template get_attribute<ValueType>(id).get_all();
        REQUIRE(attr.size() == gt.size());
        REQUIRE(std::equal(gt.begin(), gt.end(), attr.begin()));
    };

    auto check_indexed_attr =
        [](const SurfaceMesh<Scalar, Index>& mesh, AttributeId id, const std::vector<Index>& gt) {
            auto attr = mesh.template get_indexed_attribute<ValueType>(id).indices().get_all();
            REQUIRE(attr.size() == gt.size());
            REQUIRE(std::equal(gt.begin(), gt.end(), attr.begin()));
        };

    {
        // Regular mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);
        mesh.add_triangle(3, 4, 5);
        mesh.add_triangle(5, 6, 7);
        mesh.add_triangle(6, 7, 8);
        mesh.initialize_edges();
        lagrange::testing::check_mesh(mesh);

        auto vid = mesh.template create_attribute<ValueType>("vid", AttributeElement::Vertex);
        auto fid = mesh.template create_attribute<ValueType>("fid", AttributeElement::Facet);
        auto cid = mesh.template create_attribute<ValueType>("cid", AttributeElement::Corner);
        auto eid = mesh.template create_attribute<ValueType>("eid", AttributeElement::Edge);
        auto iid = mesh.template create_attribute<ValueType>("iid", AttributeElement::Indexed);
        auto xid = mesh.template create_attribute<ValueType>("xid", AttributeElement::Value);
        lagrange::testing::check_mesh(mesh);

        // Ground truth copy of each attributes
        std::vector<ValueType> vgt;
        std::vector<ValueType> fgt;
        std::vector<ValueType> cgt;
        std::vector<ValueType> egt;
        std::vector<Index> igt;

        // Initialize attribute values
        {
            auto vattr = mesh.template ref_attribute<ValueType>(vid).ref_all();
            std::iota(vattr.begin(), vattr.end(), ValueType(0));
            auto fattr = mesh.template ref_attribute<ValueType>(fid).ref_all();
            std::iota(fattr.begin(), fattr.end(), ValueType(0));
            auto cattr = mesh.template ref_attribute<ValueType>(cid).ref_all();
            std::iota(cattr.begin(), cattr.end(), ValueType(0));
            auto eattr = mesh.template ref_attribute<ValueType>(eid).ref_all();
            std::iota(eattr.begin(), eattr.end(), ValueType(0));
            auto xattr = mesh.template ref_attribute<ValueType>(xid).ref_all();
            std::iota(xattr.begin(), xattr.end(), ValueType(0));

            // For the indexed attribute, we also need to insert additional values
            auto& attr = mesh.template ref_indexed_attribute<ValueType>(iid);
            attr.values().insert_elements(mesh.get_num_corners());
            auto attr_values = attr.values().ref_all();
            auto attr_indices = attr.indices().ref_all();
            std::iota(attr_values.begin(), attr_values.end(), ValueType(0));
            std::iota(attr_indices.begin(), attr_indices.end(), Index(0));

            vgt.assign(vattr.begin(), vattr.end());
            fgt.assign(fattr.begin(), fattr.end());
            cgt.assign(cattr.begin(), cattr.end());
            egt.assign(eattr.begin(), eattr.end());
            igt.assign(attr_indices.begin(), attr_indices.end());
        }
        lagrange::testing::check_mesh(mesh);

        auto check_all_attr = [&]() {
            check_attr(mesh, fid, fgt);
            check_attr(mesh, cid, cgt);
            check_attr(mesh, vid, vgt);
            // TODO: Explicitly check edge attr remapping
            // check_attr(mesh, eid, egt);
            check_indexed_attr(mesh, iid, igt);
        };

        auto remove_facets_from_ground_truth = [&](auto func) {
            for (Index f = mesh.get_num_facets(); f-- > 0;) {
                if (func(f)) {
                    fgt.erase(fgt.begin() + f);
                    cgt.erase(cgt.begin() + f * 3, cgt.begin() + (f + 1) * 3);
                    igt.erase(igt.begin() + f * 3, igt.begin() + (f + 1) * 3);
                }
            }
        };

        auto remove_vertices_from_ground_truth = [&](auto func) {
            std::vector<bool> to_remove(mesh.get_num_facets(), false);
            for (Index f = mesh.get_num_facets(); f-- > 0;) {
                for (auto v : mesh.get_facet_vertices(f)) {
                    if (func(v)) {
                        to_remove[f] = true;
                    }
                }
            }
            for (Index v = mesh.get_num_facets(); v-- > 0;) {
                if (func(v)) {
                    vgt.erase(vgt.begin() + v);
                }
            }
            remove_facets_from_ground_truth([&](Index f) noexcept { return to_remove[f]; });
        };

        // Resize attr
        remove_facets_from_ground_truth([](Index f) noexcept { return f == 1; });
        mesh.remove_facets({1});
        lagrange::testing::check_mesh(mesh);
        check_all_attr();

        remove_facets_from_ground_truth([](Index f) noexcept { return f == 2; });
        mesh.remove_facets([](Index f) noexcept { return f == 2; });
        lagrange::testing::check_mesh(mesh);
        check_all_attr();

        remove_vertices_from_ground_truth([](Index v) noexcept { return v == 6; });
        mesh.remove_vertices([](Index v) noexcept { return v == 6; });
        lagrange::testing::check_mesh(mesh);

        remove_vertices_from_ground_truth([](Index v) noexcept { return v == 5; });
        mesh.remove_vertices({5});
        lagrange::testing::check_mesh(mesh);

        // Clear mesh
        mesh.clear_facets();
        lagrange::testing::check_mesh(mesh);
        mesh.clear_vertices();
        lagrange::testing::check_mesh(mesh);
        mesh.clear_edges();
        lagrange::testing::check_mesh(mesh);
    }

    {
        // Hybrid mesh
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(10);
        mesh.add_triangle(0, 1, 2);
        mesh.add_quad(2, 3, 4, 5);
        mesh.add_quad(2, 3, 4, 5);
        mesh.add_triangle(5, 6, 7);
        mesh.add_triangle(6, 7, 8);
        mesh.initialize_edges();
        lagrange::testing::check_mesh(mesh);

        auto vid = mesh.template create_attribute<ValueType>("vid", AttributeElement::Vertex);
        auto fid = mesh.template create_attribute<ValueType>("fid", AttributeElement::Facet);
        auto cid = mesh.template create_attribute<ValueType>("cid", AttributeElement::Corner);
        auto eid = mesh.template create_attribute<ValueType>("eid", AttributeElement::Edge);
        auto iid = mesh.template create_attribute<ValueType>("iid", AttributeElement::Indexed);
        auto xid = mesh.template create_attribute<ValueType>("xid", AttributeElement::Value);
        lagrange::testing::check_mesh(mesh);

        // Initialize attribute values
        {
            auto vattr = mesh.template ref_attribute<ValueType>(vid).ref_all();
            std::iota(vattr.begin(), vattr.end(), ValueType(0));
            auto fattr = mesh.template ref_attribute<ValueType>(fid).ref_all();
            std::iota(fattr.begin(), fattr.end(), ValueType(0));
            auto cattr = mesh.template ref_attribute<ValueType>(cid).ref_all();
            std::iota(cattr.begin(), cattr.end(), ValueType(0));
            auto eattr = mesh.template ref_attribute<ValueType>(eid).ref_all();
            std::iota(eattr.begin(), eattr.end(), ValueType(0));
            auto iattr = mesh.template ref_indexed_attribute<ValueType>(iid).values().ref_all();
            std::iota(iattr.begin(), iattr.end(), ValueType(0));
            auto xattr = mesh.template ref_attribute<ValueType>(xid).ref_all();
            std::iota(xattr.begin(), xattr.end(), ValueType(0));
        }
        lagrange::testing::check_mesh(mesh);

        // TODO: Implement "ground truth" attributes & remapping similar to the regular triangle
        // mesh (more complicated due to corner offsets).

        // Resize attr
        mesh.remove_facets({1});
        lagrange::testing::check_mesh(mesh);
        mesh.remove_facets([](Index f) noexcept { return f == 2; });
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices([](Index v) noexcept { return v == 6; });
        lagrange::testing::check_mesh(mesh);
        mesh.remove_vertices({5});
        lagrange::testing::check_mesh(mesh);

        // Clear mesh
        mesh.clear_facets();
        lagrange::testing::check_mesh(mesh);
        mesh.clear_vertices();
        lagrange::testing::check_mesh(mesh);
        mesh.clear_edges();
        lagrange::testing::check_mesh(mesh);
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_copy_attribute()
{
    using namespace lagrange;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0.0f, 0.0f, 0.0f});
    mesh.add_vertex({1.0f, 0.0f, 0.0f});
    mesh.add_vertex({0.0f, 1.0f, 0.0f});
    mesh.add_triangle(0, 1, 2);

    // Wrap a buffer as attribute.
    std::array<int, 3> indices{0, 1, 2};
    constexpr char name[] = "vertex_index";
    mesh.template wrap_as_attribute<int>(name, Vertex, AttributeUsage::Scalar, 1, indices);
    REQUIRE(mesh.has_attribute(name));
    auto& attr = mesh.template ref_attribute<int>(name);
    REQUIRE(attr.is_external());

    // "Read access"
    {
        // Copy mesh and check read-access to the attribute.
        auto mesh2 = mesh;
        REQUIRE(mesh2.has_attribute(name));
        const auto& attr2 = mesh2.template get_attribute<int>(name);
        REQUIRE(attr.get_all().data() == attr2.get_all().data());
    }

    // "AttributeCopyPolicy::CopyIfExternal"
    {
        auto mesh2 = mesh;
        // Trigger copy-on-write on the wrapped attribute.
        auto& attr2 = mesh2.template ref_attribute<int>(name);
        REQUIRE(attr.get_all().data() != attr2.get_all().data());
    }

    // "AttributeCopyPolicy::KeepExternalPtr"
    {
        attr.set_copy_policy(AttributeCopyPolicy::KeepExternalPtr);
        auto mesh2 = mesh;
        // Trigger copy-on-write on the wrapped attribute.
        auto& attr2 = mesh2.template ref_attribute<int>(name);
        REQUIRE(attr.get_all().data() == attr2.get_all().data());
    }

    // "AttributeCopyPolicy::ErrorIfExternal"
    {
        auto mesh2 = mesh;
        attr.set_copy_policy(AttributeCopyPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(mesh2.template ref_attribute<int>(name));
    }
}

template <typename Scalar, typename Index>
void test_compress_if_regular()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(3); // f0 f1 f2
    mesh.add_quads(2); // f3 f4
    mesh.add_triangles(5);
    REQUIRE(mesh.is_hybrid());
    mesh.remove_facets({3, 4});
    REQUIRE(!mesh.is_triangle_mesh());
    REQUIRE(mesh.is_hybrid());
    mesh.compress_if_regular();
    REQUIRE(mesh.is_triangle_mesh());
    REQUIRE(!mesh.is_hybrid());
}

template <typename Scalar, typename Index>
void test_shrink_to_fit()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(5);
    const Index* facet_ptr0 = mesh.get_corner_to_vertex().get_all().data();
    mesh.remove_facets({3, 4});
    const Index* facet_ptr1 = mesh.get_corner_to_vertex().get_all().data();
    REQUIRE(facet_ptr0 == facet_ptr1);
    mesh.shrink_to_fit();
    const Index* facet_ptr2 = mesh.get_corner_to_vertex().get_all().data();
    REQUIRE(facet_ptr0 != facet_ptr2);
}

template <typename Scalar, typename Index>
void test_1_and_2_facets()
{
    SECTION("add_polygon(facet_size)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});
        mesh.add_polygon(1);
        mesh.add_polygon(2);
        mesh.add_polygon(3);
        mesh.add_polygon(4);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("add_polygon(span<> facet_indices)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});

        const Index a[]{0};
        const Index b[]{0, 1};
        const Index c[]{1, 2};
        const Index d[]{2, 0};
        mesh.add_polygon(a);
        mesh.add_polygon(b);
        mesh.add_polygon(c);
        mesh.add_polygon(d);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("add_polygon(initializer_list<> facet_indices)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});

        mesh.add_polygon({0});
        mesh.add_polygon({0, 1});
        mesh.add_polygon({1, 2});
        mesh.add_polygon({2, 0});
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("add_polygon(facet_size, set_func)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});

        mesh.add_polygon(1, [](lagrange::span<Index> t) { t[0] = 0; });
        mesh.add_polygon(2, [](lagrange::span<Index> t) {
            t[0] = 0;
            t[1] = 1;
        });
        mesh.add_polygon(2, [](lagrange::span<Index> t) {
            t[0] = 1;
            t[1] = 2;
        });
        mesh.add_polygon(2, [](lagrange::span<Index> t) {
            t[0] = 2;
            t[1] = 0;
        });
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("add_polygons(facet_num, facet_size, span)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});

        const Index a[]{0};
        mesh.add_polygons(1, 1, a);

        const Index b[]{0, 1, 1, 2, 2, 0};
        mesh.add_polygons(3, 2, b);

        lagrange::testing::check_mesh(mesh);
    }

    SECTION("add_polygons(facet_num, facet_size, fn)")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0f, 0.0f, 0.0f});
        mesh.add_vertex({1.0f, 0.0f, 0.0f});
        mesh.add_vertex({0.0f, 1.0f, 0.0f});

        // Three 2-facets
        mesh.add_polygons(3, 2, [](Index f, lagrange::span<Index> t) {
            t[0] = f % 3;
            t[1] = (f + 1) % 3;
        });

        // Two 1-facets
        mesh.add_polygons(2, 1, [](Index /*f*/, lagrange::span<Index> t) { t[0] = 0; });

        lagrange::testing::check_mesh(mesh);
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_value_attribute()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.add_triangles(5);

    ValueType values[6] = {1, 2, 3, 4, 5, 6};

    auto id = mesh.template create_attribute<ValueType>(
        "value",
        lagrange::AttributeElement::Value,
        lagrange::AttributeUsage::Vector,
        2,
        {values, 6});
    REQUIRE(mesh.has_attribute("value"));
    REQUIRE(mesh.get_attribute_id("value") == id);

    auto& attr = mesh.template get_attribute<ValueType>(id);
    REQUIRE(attr.get_num_elements() == 3);
    REQUIRE(attr.get_num_channels() == 2);
    REQUIRE(attr.get_usage() == lagrange::AttributeUsage::Vector);
    REQUIRE(attr.get_element_type() == lagrange::AttributeElement::Value);

    for (ValueType i = 0; i < 3; i++) {
        REQUIRE(attr.get(i, 0) == i * 2 + 1);
        REQUIRE(attr.get(i, 1) == i * 2 + 2);
    }

    lagrange::testing::check_mesh(mesh);
}

} // namespace

TEST_CASE("SurfaceMesh Construction", "[next]")
{
#define LA_X_test_mesh_construction(_, Scalar, Index) test_mesh_construction<Scalar, Index>();
    LA_SURFACE_MESH_X(test_mesh_construction, 0)
}

TEST_CASE("SurfaceMesh: Remove Elements", "[next]")
{
    SECTION("Without edges")
    {
#define LA_X_test_element_removal_false(_, Scalar, Index) \
    test_element_removal<Scalar, Index>(false);
        LA_SURFACE_MESH_X(test_element_removal_false, 0)
    }
    SECTION("With edges")
    {
#define LA_X_test_element_removal_true(_, Scalar, Index) test_element_removal<Scalar, Index>(true);
        LA_SURFACE_MESH_X(test_element_removal_true, 0)
    }
}

TEST_CASE("SurfaceMesh: Storage", "[next]")
{
#define LA_X_test_mesh_storage(_, Scalar, Index) test_mesh_storage<Scalar, Index>();
    LA_SURFACE_MESH_X(test_mesh_storage, 0)
}

TEST_CASE("SurfaceMesh: Copy and Move", "[next]")
{
    SECTION("Without edges")
    {
#define LA_X_test_copy_move_false(_, Scalar, Index) test_copy_move<Scalar, Index>(false);
        LA_SURFACE_MESH_X(test_copy_move_false, 0)
    }
    SECTION("With edges")
    {
#define LA_X_test_copy_move_true(_, Scalar, Index) test_copy_move<Scalar, Index>(true);
        LA_SURFACE_MESH_X(test_copy_move_true, 0)
    }
}

TEST_CASE("SurfaceMesh: Create Attribute", "[next]")
{
#define LA_X_test_mesh_attribute(ValueType, Scalar, Index) \
    test_mesh_attribute<ValueType, Scalar, Index>();
#define LA_X_test_mesh_attribute_aux(_, ValueType) LA_SURFACE_MESH_X(test_mesh_attribute, ValueType)
    LA_ATTRIBUTE_X(test_mesh_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: Normal Attribute", "[next]")
{
#define LA_X_test_normal_attribute(ValueType, Scalar, Index) \
    test_normal_attribute<ValueType, Scalar, Index>();
#define LA_X_test_normal_attribute_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_normal_attribute, ValueType)
    LA_ATTRIBUTE_X(test_normal_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: Wrap Attribute", "[next]")
{
#define LA_X_test_wrap_attribute(ValueType, Scalar, Index) \
    test_wrap_attribute<ValueType, Scalar, Index>();
#define LA_X_test_wrap_attribute_aux(_, ValueType) LA_SURFACE_MESH_X(test_wrap_attribute, ValueType)
    LA_ATTRIBUTE_X(test_wrap_attribute_aux, 0)

#define LA_X_test_wrap_attribute_special(_, Scalar, Index) \
    test_wrap_attribute_special<Scalar, Index>();
    LA_SURFACE_MESH_X(test_wrap_attribute_special, 0)
}

TEST_CASE("SurfaceMesh: Export Attribute", "[next]")
{
#define LA_X_test_export_attribute(ValueType, Scalar, Index) \
    test_export_attribute<ValueType, Scalar, Index>();
#define LA_X_test_export_attribute_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_export_attribute, ValueType)
    LA_ATTRIBUTE_X(test_export_attribute_aux, 0)

#define LA_X_test_export_attribute_special(_, Scalar, Index) \
    test_export_attribute_special<Scalar, Index>();
    LA_SURFACE_MESH_X(test_export_attribute_special, 0)
}

TEST_CASE("SurfaceMesh: Indexed Attribute", "[next]")
{
#define LA_X_test_indexed_attribute(ValueType, Scalar, Index) \
    test_indexed_attribute<ValueType, Scalar, Index>();
#define LA_X_test_indexed_attribute_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_indexed_attribute, ValueType)
    LA_ATTRIBUTE_X(test_indexed_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: Resize Attribute Basic", "[next]")
{
#define LA_X_test_resize_attribute_basic(_, Scalar, Index) \
    test_resize_attribute_basic<Scalar, Index>();
    LA_SURFACE_MESH_X(test_resize_attribute_basic, 0)
}

TEST_CASE("SurfaceMesh: Edit Facets With Edges", "[next]")
{
#define LA_X_test_edit_facets_with_edges(_, Scalar, Index) \
    test_edit_facets_with_edges<Scalar, Index>();
    LA_SURFACE_MESH_X(test_edit_facets_with_edges, 0)
}

TEST_CASE("SurfaceMesh: User Edges", "[next]")
{
#define LA_X_test_user_edges(_, Scalar, Index) test_user_edges<Scalar, Index>();
    LA_SURFACE_MESH_X(test_user_edges, 0)
}

TEST_CASE("SurfaceMesh: Reserved Attributes Basic", "[next]")
{
#define LA_X_test_reserved_attribute_basic(_, Scalar, Index) \
    test_reserved_attribute_basic<Scalar, Index>();
    LA_SURFACE_MESH_X(test_reserved_attribute_basic, 0)
}

TEST_CASE("SurfaceMesh: Custom Reserved Attributes", "[next]")
{
#define LA_X_test_custom_reserved_attributes(ValueType, Scalar, Index) \
    test_custom_reserved_attributes<ValueType, Scalar, Index>();
#define LA_X_test_custom_reserved_attributes_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_custom_reserved_attributes, ValueType)
    LA_ATTRIBUTE_X(test_custom_reserved_attributes_aux, 0)
}

TEST_CASE("SurfaceMesh: Element Index Type", "[next]")
{
#define LA_X_test_element_index_type(ValueType, Scalar, Index) \
    test_element_index_type<ValueType, Scalar, Index>();
#define LA_X_test_element_index_type_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_element_index_type, ValueType)
    LA_ATTRIBUTE_X(test_element_index_type_aux, 0)
}

TEST_CASE("SurfaceMesh: Element Index Resize", "[next]")
{
#define LA_X_test_element_index_resize(_, Scalar, Index) test_element_index_resize<Scalar, Index>();
    LA_SURFACE_MESH_X(test_element_index_resize, 0)
}

TEST_CASE("SurfaceMesh: Resize Attribute Type", "[next]")
{
#define LA_X_test_resize_attribute_type(ValueType, Scalar, Index) \
    test_resize_attribute_type<ValueType, Scalar, Index>();
#define LA_X_test_resize_attribute_type_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_resize_attribute_type, ValueType)
    LA_ATTRIBUTE_X(test_resize_attribute_type_aux, 0)
}

TEST_CASE("SurfaceMesh: Copy Attribute", "[next]")
{
#define LA_X_test_copy_attribute(ValueType, Scalar, Index) \
    test_copy_attribute<ValueType, Scalar, Index>();
#define LA_X_test_copy_attribute_aux(_, ValueType) LA_SURFACE_MESH_X(test_copy_attribute, ValueType)
    LA_ATTRIBUTE_X(test_copy_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: Shrink To Fit", "[next]")
{
#define LA_X_test_shrink_to_fit(_, Scalar, Index) test_shrink_to_fit<Scalar, Index>();
    LA_SURFACE_MESH_X(test_shrink_to_fit, 0)
}

TEST_CASE("SurfaceMesh: Compress If Regular", "[next]")
{
#define LA_X_test_compress_if_regular(_, Scalar, Index) test_compress_if_regular<Scalar, Index>();
    LA_SURFACE_MESH_X(test_compress_if_regular, 0)
}

TEST_CASE("SurfaceMesh: Test facets of size 1 and 2", "[next]")
{
#define LA_X_test_1_and_2_facets(_, Scalar, Index) test_1_and_2_facets<Scalar, Index>();
    LA_SURFACE_MESH_X(test_1_and_2_facets, 0)
}

TEST_CASE("SurfaceMesh: Value Attribute", "[next]")
{
#define LA_X_test_value_attribute(ValueType, Scalar, Index) \
    test_value_attribute<ValueType, Scalar, Index>();
#define LA_X_test_value_attribute_aux(_, ValueType) \
    LA_SURFACE_MESH_X(test_value_attribute, ValueType)
    LA_ATTRIBUTE_X(test_value_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: sanity check", "[next]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Simple cube:
    //  3 +---+ 2
    //    | / |
    //  0 +---+ 1
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);
    mesh.initialize_edges();

    lagrange::testing::check_mesh(mesh);
}
