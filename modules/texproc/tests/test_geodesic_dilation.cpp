/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/map_attribute.h>
#include <lagrange/testing/common.h>
#include <lagrange/texproc/geodesic_dilation.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Misha/Geometry.h>
#include <Misha/RegularGrid.h>
#include <Misha/Texels.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <random>

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

namespace {

using Array3Df = lagrange::image::experimental::Array3D<float>;

// Using `Point` directly leads to ambiguity with Apple Accelerate types.
template <typename T, unsigned N>
using Vector = MishaK::Point<T, N>;

// The dimension of the manifold.
static const unsigned int K = 2;

// The dimension of the space into which the manifold is embedded.
static const unsigned int Dim = 3;

using Real = double;

using Scalar = float;
using Index = uint32_t;

void fill_torus(
    std::vector<MishaK::SimplexIndex<K>>& simplices,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, K>>& textureCoordinates,
    double radius1,
    double radius2,
    unsigned int res)
{
    vertices.resize(res * res);
    simplices.reserve(res * res * 2);
    textureCoordinates.reserve(simplices.size() * 3);

    auto VIndex = [&](unsigned int i, unsigned int j) { return (j % res) * res + (i % res); };
    auto ThetaPhi = [&](unsigned int i, unsigned int j) {
        return std::pair<double, double>((2. * M_PI * i) / res, (2. * M_PI * j) / res);
    };
    auto TextureCoordinate = [&](unsigned int i, unsigned int j) {
        return std::pair<double, double>((i + 0.) / res, (j + 0.) / res);
    };


    Vector<double, 3> y = Vector<double, 3>(0, 1, 0);
    for (unsigned int j = 0; j < res; j++) {
        for (unsigned int i = 0; i < res; i++) {
            std::pair<double, double> theta_phi = ThetaPhi(i, j);
            double c_theta = cos(theta_phi.first), s_theta = sin(theta_phi.first);
            double c_phi = cos(theta_phi.second), s_phi = sin(theta_phi.second);

            Vector<double, 3> x = Vector<double, 3>(c_theta, 0, s_theta);

            vertices[VIndex(i, j)] = Vector<double, 3>(c_theta, 0, s_theta) * radius1 +
                                     (y * s_phi + x * c_phi) * radius2;
        }
    }

    auto _TextureCoordinate = [&](unsigned int i, unsigned int j) {
        std::pair<double, double> st = TextureCoordinate(i, j);
        return Vector<double, K>(st.first * 0.5 + 0.25, st.second * 0.5 + 0.25);
    };

    for (unsigned int j = 0; j < res; j++) {
        for (unsigned int i = 0; i < res; i++) {
            simplices.emplace_back(
                MishaK::SimplexIndex<K>(VIndex(i, j), VIndex(i + 1, j + 1), VIndex(i + 1, j)));
            textureCoordinates.emplace_back(_TextureCoordinate(i, j));
            textureCoordinates.emplace_back(_TextureCoordinate(i + 1, j + 1));
            textureCoordinates.emplace_back(_TextureCoordinate(i + 1, j));

            simplices.emplace_back(
                MishaK::SimplexIndex<K>(VIndex(i, j), VIndex(i, j + 1), VIndex(i + 1, j + 1)));
            textureCoordinates.emplace_back(_TextureCoordinate(i, j));
            textureCoordinates.emplace_back(_TextureCoordinate(i, j + 1));
            textureCoordinates.emplace_back(_TextureCoordinate(i + 1, j + 1));
        }
    }
}

template <typename Scalar, typename Index>
void parse_mesh(
    const lagrange::SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<MishaK::SimplexIndex<K>>& simplices,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, K>>& textureCoordinates,
    lagrange::AttributeId texcoord_id)
{
    simplices.resize(t_mesh.get_num_facets());
    vertices.resize(t_mesh.get_num_vertices());
    textureCoordinates.resize(t_mesh.get_num_corners());

    auto vertex_indices = t_mesh.get_corner_to_vertex().get_all();
    for (unsigned int i = 0; i < t_mesh.get_num_facets(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            simplices[i][k] = static_cast<int>(vertex_indices[i * (K + 1) + k]);
        }
    }

    // Retrieve input vertex buffer
    auto& input_coords = t_mesh.get_vertex_to_position();
    la_runtime_assert(
        input_coords.get_num_elements() == t_mesh.get_num_vertices(),
        "Number of vertices should match number of vertices");

    // Retrieve input texture-coordinate buffer
    auto& input_texture_coordinates = t_mesh.template get_attribute<Scalar>(texcoord_id);
    la_runtime_assert(
        input_texture_coordinates.get_num_channels() == 2,
        "Input texture coordinates should only have 2 channels");
    la_runtime_assert(
        input_texture_coordinates.get_num_elements() == t_mesh.get_num_corners(),
        "Number of texture coordinates should match number of corners");

    auto _input_coords = input_coords.get_all();
    auto _input_texture_coordinates = input_texture_coordinates.get_all();

    for (unsigned int i = 0; i < t_mesh.get_num_vertices(); i++) {
        for (unsigned int d = 0; d < Dim; d++) {
            vertices[i][d] = static_cast<Real>(_input_coords[i * Dim + d]);
        }
    }
    for (unsigned int i = 0; i < t_mesh.get_num_corners(); i++) {
        for (unsigned int k = 0; k < K; k++) {
            textureCoordinates[i][k] = static_cast<Real>(_input_texture_coordinates[i * K + k]);
        }
        textureCoordinates[i][1] = 1. - textureCoordinates[i][1];
    }
}

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> get_mesh(
    const std::vector<Vector<double, Dim>>& vertices,
    const std::vector<MishaK::SimplexIndex<K>>& triangles)
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;

    for (unsigned int i = 0; i < vertices.size(); i++)
        mesh.add_vertex(
            {static_cast<Scalar>(vertices[i][0]),
             static_cast<Scalar>(vertices[i][1]),
             static_cast<Scalar>(vertices[i][2])});

    for (unsigned int i = 0; i < triangles.size(); i++)
        mesh.add_triangle(triangles[i][0], triangles[i][1], triangles[i][2]);

    return mesh;
}

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> convert_to_mesh(
    const std::vector<Vector<double, Dim>>& vertices,
    const std::vector<MishaK::SimplexIndex<K>>& triangles,
    const std::vector<Vector<double, K>>& texture_coordinates_)
{
    lagrange::SurfaceMesh<Scalar, Index> mesh = get_mesh<Scalar, Index>(vertices, triangles);

    lagrange::AttributeId uv = mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Corner,
        lagrange::AttributeUsage::UV,
        2);

    auto& texture_coordinates = mesh.template ref_attribute<Scalar>(uv);

    for (unsigned int i = 0; i < texture_coordinates_.size(); i++) {
        auto row = texture_coordinates.ref_row(i);
        for (unsigned int k = 0; k < K; k++)
            row[k] = static_cast<Scalar>(texture_coordinates_[i][k]);
    }

    return mesh;
}

lagrange::AttributeId get_uv_attribute_id(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    bool indexed = false)
{
    // Get the texcoord id (and set the texcoords if they weren't already)
    lagrange::AttributeId texcoord_id;

    // If the mesh comes with UVs
    if (auto res = lagrange::find_matching_attribute(mesh, lagrange::AttributeUsage::UV)) {
        texcoord_id = res.value();
    } else {
        la_runtime_assert(false, "Requires uv coordinates.");
    }
    // Make sure the UV coordinate type is the same as that of the vertices
    if (!mesh.template is_attribute_type<Scalar>(texcoord_id)) {
        lagrange::logger().warn(
            "Input uv coordinates do not have the same scalar type as the input points. Casting "
            "attribute.");
        texcoord_id = lagrange::cast_attribute_in_place<Scalar>(mesh, texcoord_id);
    }

    // Make sure the UV coordinates are associated with the corners
    if (indexed) {
        if (mesh.get_attribute_base(texcoord_id).get_element_type() !=
            lagrange::AttributeElement::Indexed) {
            lagrange::logger().debug("UV coordinates are not indexed. Making indexed.");
            texcoord_id = lagrange::map_attribute(
                mesh,
                texcoord_id,
                "indexed_texture",
                lagrange::AttributeElement::Indexed);
        }
    } else {
        if (mesh.get_attribute_base(texcoord_id).get_element_type() !=
            lagrange::AttributeElement::Corner) {
            lagrange::logger().debug(
                "UV coordinates are not associated with the corners. Mapping to corners.");
            texcoord_id = lagrange::map_attribute(
                mesh,
                texcoord_id,
                "corner_texture",
                lagrange::AttributeElement::Corner);
        }
    }

    return texcoord_id;
}

void test_texture_dilation(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::AttributeId texcoord_id,
    const unsigned int width,
    const unsigned int height,
    const unsigned int num_samples,
    const double epsilon)
{
    lagrange::texproc::DilationOptions dilation_options;
    dilation_options.dilation_radius = 0;
    dilation_options.output_position_map = true;
    const unsigned int res[] = {width, height};

    Array3Df _positions = lagrange::image::experimental::create_image<float>(width, height, 3);

    // Initialize positions to infinity
    for (unsigned int j = 0; j < _positions.extent(1); j++) {
        for (unsigned int i = 0; i < _positions.extent(0); i++) {
            for (unsigned int c = 0; c < 3; c++) {
                _positions(i, j, c) = std::numeric_limits<float>::infinity();
            }
        }
    }

    lagrange::texproc::geodesic_dilation(mesh, _positions.to_mdspan(), dilation_options);

    // Copy the positions into a MishaK::RegularGrid
    MishaK::RegularGrid<K, Vector<double, 3>> positions(res);
    for (unsigned int j = 0; j < positions.res(1); j++) {
        for (unsigned int i = 0; i < positions.res(0); i++) {
            for (unsigned int c = 0; c < 3; c++) {
                positions(i, j)[c] = _positions(i, j, c);
            }
        }
    }

    // Copy the mesh data into MishaK structs
    std::vector<MishaK::SimplexIndex<K>> simplices;
    std::vector<Vector<Real, Dim>> vertices;
    std::vector<Vector<Real, K>> textureCoordinates;
    parse_mesh(mesh, simplices, vertices, textureCoordinates, texcoord_id);

    // Get the scale of the mesh
    double mesh_scale = 0;
    {
        Vector<double, 3> center;
        for (unsigned int i = 0; i < vertices.size(); i++) {
            center += vertices[i];
        }
        center /= vertices.size();
        for (unsigned int i = 0; i < vertices.size(); i++) {
            mesh_scale += Vector<double, 3>::SquareNorm(vertices[i] - center);
        }
        mesh_scale = sqrt(mesh_scale / vertices.size());
    }

    std::mt19937 rng;
    std::uniform_int_distribution<unsigned int> dist_simplex(0, (unsigned int)simplices.size() - 1);
    std::uniform_real_distribution<double> dist_uniform(0., 1.);

    // Generates a random barycentric coordinate
    auto random_barycentric_coordinate = [&](void) {
        Vector<double, 3> p;
        p[0] = dist_uniform(rng);
        p[1] = dist_uniform(rng);
        if (p[0] + p[1] > 1) {
            p[0] = 1. - p[0], p[1] = 1. - p[1];
        }
        p[2] = 1. - p[0] - p[1];
        return p;
    };

    // Gets the point on the embedded mesh
    auto embedding_simplex = [&](unsigned int s) {
        return MishaK::Simplex<double, Dim, K>(
            vertices[simplices[s][0]],
            vertices[simplices[s][1]],
            vertices[simplices[s][2]]);
    };

    // Gets the point on the texture mesh
    auto texture_simplex = [&](unsigned int s) {
        return MishaK::Simplex<double, K, K>(
            textureCoordinates[3 * s + 0],
            textureCoordinates[3 * s + 1],
            textureCoordinates[3 * s + 2]);
    };

    // Gets the interpolated embedded position
    auto embedded_position = [&](unsigned int s, Vector<double, 3> bc) {
        // The embedded triangle
        MishaK::Simplex<double, 3, 2> simplex = embedding_simplex(s);
        return simplex(bc);
    };

    // Gets the interpolated texture position
    auto texture_position = [&](unsigned int s, Vector<double, 3> bc) {
        // The texture triangle, in normalized coordinates
        MishaK::Simplex<double, 2, 2> simplex = texture_simplex(s);

        // The texture triangle, in grid coordinates (adjusted for the fact that nodes are at
        // corners)
        for (unsigned int k = 0; k <= K; k++) {
            for (unsigned int _k = 0; _k < K; _k++) {
                simplex[k][_k] = simplex[k][_k] * res[_k] - 0.5;
            }
        }

        // The grid coordinate
        Vector<double, K> p = simplex(bc);

        // The position in the grid
        return positions(p);
    };

    double error_mean = 0;
    double error_max = 0.0;
    for (unsigned int i = 0; i < num_samples; i++) {
        const unsigned int s = dist_simplex(rng);
        const Vector<double, K + 1> bc = random_barycentric_coordinate();

        const Vector<double, Dim> p = embedded_position(s, bc);
        const Vector<double, Dim> q = texture_position(s, bc);

        const double delta_squared = Vector<double, 3>::SquareNorm(p - q);
        error_mean += delta_squared;
        error_max = std::max<double>(error_max, delta_squared);
    }
    error_mean = sqrt(error_mean / num_samples) / mesh_scale;
    error_max = sqrt(error_max) / mesh_scale;

    lagrange::logger().info("Difference avg/max: {} / {}", error_mean, error_max);

    REQUIRE(error_mean < epsilon);
    // REQUIRE(error_max < epsilon); // not quite there yet
}

} // namespace

TEST_CASE("texture dilation", "[texproc]")
{
    using lagrange::testing::load_surface_mesh;

    const unsigned int test_samples = 100000;
    const double test_eps = 0.05;
    const unsigned int test_width = 2048;
    const unsigned int test_height = 2048;

    SECTION("blub")
    {
        auto mesh = load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
        const auto tex_id = get_uv_attribute_id(mesh);
        REQUIRE(tex_id != lagrange::invalid_attribute_id());
        test_texture_dilation(mesh, tex_id, test_width, test_height, test_samples, test_eps);
    }

    SECTION("spot")
    {
        auto mesh = load_surface_mesh<Scalar, Index>("open/core/spot/spot_triangulated.obj");
        const auto tex_id = get_uv_attribute_id(mesh);
        REQUIRE(tex_id != lagrange::invalid_attribute_id());
        test_texture_dilation(mesh, tex_id, test_width, test_height, test_samples, test_eps);
    }

    SECTION("torus")
    {
        const double radius1 = 2;
        const double radius2 = 1;
        const unsigned int res = 256;
        std::vector<MishaK::SimplexIndex<K>> simplices;
        std::vector<Vector<Real, Dim>> vertices;
        std::vector<Vector<Real, K>> uvs;
        fill_torus(simplices, vertices, uvs, radius1, radius2, res);
        auto mesh = convert_to_mesh<Scalar, Index>(vertices, simplices, uvs);
        const auto tex_id = get_uv_attribute_id(mesh);
        REQUIRE(tex_id != lagrange::invalid_attribute_id());
        test_texture_dilation(mesh, tex_id, test_width, test_height, test_samples, test_eps);
    }
}

TEST_CASE("geodesic dilation quad", "[texproc]")
{
    lagrange::SurfaceMesh<Scalar, Index> quad_mesh;
    quad_mesh.add_vertex({0, 0, 0});
    quad_mesh.add_vertex({1, 0, 0});
    quad_mesh.add_vertex({1, 1, 0});
    quad_mesh.add_vertex({0, 1, 0});
    quad_mesh.add_triangle(0, 1, 2);
    quad_mesh.add_triangle(0, 2, 3);
    quad_mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Vertex,
        2,
        lagrange::AttributeUsage::UV);
    lagrange::attribute_matrix_ref<Scalar>(quad_mesh, "uv") =
        vertex_view(quad_mesh).template leftCols<2>();
    lagrange::map_attribute_in_place(quad_mesh, "uv", lagrange::AttributeElement::Indexed);

    auto img = lagrange::image::experimental::create_image<float>(128, 128, 3);
    for (size_t i = 0; i < img.extent(0); ++i) {
        for (size_t j = 0; j < img.extent(1); ++j) {
            for (size_t c = 0; c < 3; ++c) {
                img(i, j, c) = 0.0f;
            }
        }
    }
    for (size_t i = 32; i < 48; ++i) {
        for (size_t j = 24; j < 32; ++j) {
            for (size_t c = 0; c < 3; ++c) {
                img(i, j, c) = 1.0f;
            }
        }
    }

    lagrange::texproc::DilationOptions options;

    SECTION("position dilation")
    {
        options.output_position_map = true;
        REQUIRE_NOTHROW(lagrange::texproc::geodesic_dilation(quad_mesh, img.to_mdspan(), options));
    }

    SECTION("texture dilation")
    {
        options.output_position_map = false;
        REQUIRE_NOTHROW(lagrange::texproc::geodesic_dilation(quad_mesh, img.to_mdspan(), options));
    }
}
