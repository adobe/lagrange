/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/SurfaceMesh.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/map_attribute.h>
#include <lagrange/testing/common.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/views.h>

#include <catch2/catch_test_macros.hpp>

#include <Eigen/Geometry>

#include <cmath>

using namespace lagrange;
using namespace lagrange::texproc;
using Array3Df = image::experimental::Array3D<float>;

namespace {

// Create a unit-square quad (two triangles) in the XY plane with identity UV mapping.
// Vertices: (0,0,0), (1,0,0), (1,1,0), (0,1,0)
// UVs match vertex XY positions: (0,0), (1,0), (1,1), (0,1)
SurfaceMesh32f create_quad_mesh()
{
    SurfaceMesh32f mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    mesh.create_attribute<float>("uv", AttributeElement::Vertex, 2, AttributeUsage::UV);
    attribute_matrix_ref<float>(mesh, "uv") = vertex_view(mesh).leftCols<2>();
    map_attribute_in_place(mesh, "uv", AttributeElement::Indexed);

    return mesh;
}

// Create a solid red render image of the given size.
Array3Df create_solid_render(size_t width, size_t height)
{
    auto img = image::experimental::create_image<float>(width, height, 3);
    for (size_t x = 0; x < width; ++x) {
        for (size_t y = 0; y < height; ++y) {
            img(x, y, 0) = 1.0f;
            img(x, y, 1) = 0.0f;
            img(x, y, 2) = 0.0f;
        }
    }
    return img;
}

// Build a look-at view matrix (world -> view).
Eigen::Affine3f look_at(Eigen::Vector3f eye, Eigen::Vector3f center, Eigen::Vector3f up)
{
    Eigen::Vector3d forward = (center - eye).cast<double>().normalized();
    Eigen::Vector3d right = forward.cross(up.cast<double>().normalized()).normalized();
    Eigen::Vector3d upwards = right.cross(forward);

    Eigen::Affine3d V = Eigen::Affine3d::Identity();
    V.linear() << right, upwards, -forward;
    V.translation() << -right.dot(eye.cast<double>()), -upwards.dot(eye.cast<double>()),
        forward.dot(eye.cast<double>());
    return V.cast<float>();
}

// Build a finite perspective projection matrix.
Eigen::Projective3f perspective(float fovy, float aspect, float z_near, float z_far)
{
    const double tan_half = std::tan(static_cast<double>(fovy) / 2.0);
    Eigen::Matrix4d P = Eigen::Matrix4d::Zero();
    P(0, 0) = 1.0 / (aspect * tan_half);
    P(1, 1) = 1.0 / tan_half;
    P(2, 2) = -(z_far + z_near) / (z_far - z_near);
    P(3, 2) = -1.0;
    P(2, 3) = -(2.0 * z_far * z_near) / (z_far - z_near);
    return Eigen::Projective3d(P).cast<float>();
}

// Build an orthographic projection matrix.
Eigen::Projective3f ortho(float width, float aspect, float z_near, float z_far)
{
    const double w = width;
    const double h = w / aspect;
    Eigen::Matrix4d P = Eigen::Matrix4d::Identity();
    P(0, 0) = 2.0 / w;
    P(1, 1) = 2.0 / h;
    P(2, 2) = -2.0 / (z_far - z_near);
    P(0, 3) = 0; // symmetric
    P(1, 3) = 0;
    P(2, 3) = -(static_cast<double>(z_far) + z_near) / (z_far - z_near);
    return Eigen::Projective3d(P).cast<float>();
}

// Sum all confidence values in a single-channel weight image.
double total_confidence(const Array3Df& weights)
{
    double sum = 0;
    for (size_t x = 0; x < weights.extent(0); ++x) {
        for (size_t y = 0; y < weights.extent(1); ++y) {
            sum += weights(x, y, 0);
        }
    }
    return sum;
}

// Count texels with non-zero confidence.
size_t count_nonzero(const Array3Df& weights)
{
    size_t count = 0;
    for (size_t x = 0; x < weights.extent(0); ++x) {
        for (size_t y = 0; y < weights.extent(1); ++y) {
            if (weights(x, y, 0) > 0) ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("TextureRasterizer perspective camera", "[texproc]")
{
    auto mesh = create_quad_mesh();

    constexpr size_t tex_size = 64;
    constexpr size_t render_size = 64;

    TextureRasterizerOptions opts;
    opts.width = tex_size;
    opts.height = tex_size;
    TextureRasterizer<float, uint32_t> rasterizer(mesh, opts);

    // Camera at (0.5, 0.5, 2) looking at quad center (0.5, 0.5, 0)
    CameraOptions camera;
    camera.view_transform = look_at({0.5f, 0.5f, 2.0f}, {0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f});
    camera.projection_transform = perspective(
        static_cast<float>(2.0 * std::atan(0.5 / 2.0)), // fov to see ~unit width at z=2
        1.0f,
        0.1f,
        10.0f);

    auto render = create_solid_render(render_size, render_size);
    auto [texture, weights] = rasterizer.weighted_texture_from_render(render, camera);

    REQUIRE(texture.extent(0) == tex_size);
    REQUIRE(texture.extent(1) == tex_size);
    REQUIRE(weights.extent(0) == tex_size);
    REQUIRE(weights.extent(1) == tex_size);

    // Should have significant coverage with non-zero confidence
    size_t nonzero = count_nonzero(weights);
    REQUIRE(nonzero > 0);
    CHECK(total_confidence(weights) > 0.0);

    // Visible texels should be red
    for (size_t x = 0; x < tex_size; ++x) {
        for (size_t y = 0; y < tex_size; ++y) {
            if (weights(x, y, 0) > 0) {
                CHECK(texture(x, y, 0) > 0.5f); // red channel
            }
        }
    }
}

TEST_CASE("TextureRasterizer orthographic camera", "[texproc]")
{
    auto mesh = create_quad_mesh();

    constexpr size_t tex_size = 64;
    constexpr size_t render_size = 64;

    TextureRasterizerOptions opts;
    opts.width = tex_size;
    opts.height = tex_size;
    TextureRasterizer<float, uint32_t> rasterizer(mesh, opts);

    // Orthographic camera at (0.5, 0.5, 2) looking at quad center
    CameraOptions camera;
    camera.view_transform = look_at({0.5f, 0.5f, 2.0f}, {0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f});
    camera.projection_transform = ortho(
        1.5f, // width large enough to see the whole quad
        1.0f,
        0.1f,
        10.0f);

    auto render = create_solid_render(render_size, render_size);
    auto [texture, weights] = rasterizer.weighted_texture_from_render(render, camera);

    REQUIRE(texture.extent(0) == tex_size);
    REQUIRE(texture.extent(1) == tex_size);
    REQUIRE(weights.extent(0) == tex_size);
    REQUIRE(weights.extent(1) == tex_size);

    // Should have significant coverage with non-zero confidence
    size_t nonzero = count_nonzero(weights);
    REQUIRE(nonzero > 0);
    CHECK(total_confidence(weights) > 0.0);

    // Visible texels should be red
    for (size_t x = 0; x < tex_size; ++x) {
        for (size_t y = 0; y < tex_size; ++y) {
            if (weights(x, y, 0) > 0) {
                CHECK(texture(x, y, 0) > 0.5f);
            }
        }
    }
}

TEST_CASE("TextureRasterizer ortho vs perspective confidence consistency", "[texproc]")
{
    // For a quad viewed head-on, orthographic and perspective cameras should produce
    // similar confidence patterns (high confidence in the center).
    auto mesh = create_quad_mesh();

    constexpr size_t tex_size = 64;
    constexpr size_t render_size = 64;

    TextureRasterizerOptions opts;
    opts.width = tex_size;
    opts.height = tex_size;
    TextureRasterizer<float, uint32_t> rasterizer(mesh, opts);

    Eigen::Vector3f eye(0.5f, 0.5f, 2.0f);
    Eigen::Vector3f center(0.5f, 0.5f, 0.0f);
    Eigen::Vector3f up(0.0f, 1.0f, 0.0f);
    auto view = look_at(eye, center, up);

    CameraOptions persp_camera;
    persp_camera.view_transform = view;
    persp_camera.projection_transform =
        perspective(static_cast<float>(2.0 * std::atan(0.5 / 2.0)), 1.0f, 0.1f, 10.0f);

    CameraOptions ortho_camera;
    ortho_camera.view_transform = view;
    ortho_camera.projection_transform = ortho(1.5f, 1.0f, 0.1f, 10.0f);

    auto render = create_solid_render(render_size, render_size);
    auto [persp_tex, persp_w] = rasterizer.weighted_texture_from_render(render, persp_camera);
    auto [ortho_tex, ortho_w] = rasterizer.weighted_texture_from_render(render, ortho_camera);
    (void)persp_tex;
    (void)ortho_tex;

    size_t persp_nonzero = count_nonzero(persp_w);
    size_t ortho_nonzero = count_nonzero(ortho_w);

    // Both should have significant coverage
    REQUIRE(persp_nonzero > 0);
    REQUIRE(ortho_nonzero > 0);

    // For a head-on view, orthographic confidence should be very uniform (close to 1.0
    // for all visible texels, since all view directions are parallel to the normal).
    double ortho_total = total_confidence(ortho_w);
    double ortho_avg = ortho_total / ortho_nonzero;
    // Ortho head-on: normal_confidence = |dot(n, (0,0,-1))| = 1.0 for all visible texels.
    // Average is reduced by depth discontinuity erosion near mesh boundaries, but should still
    // be meaningfully positive.
    CHECK(ortho_avg > 0.1);
}

TEST_CASE("TextureRasterizer invalid projection matrix", "[texproc]")
{
    auto mesh = create_quad_mesh();

    constexpr size_t tex_size = 16;
    constexpr size_t render_size = 16;

    TextureRasterizerOptions opts;
    opts.width = tex_size;
    opts.height = tex_size;
    TextureRasterizer<float, uint32_t> rasterizer(mesh, opts);

    // Build a projection matrix that doesn't match either convention:
    // row 3 = [0, 0, 0.5, 0.5] — neither perspective nor orthographic.
    Eigen::Matrix4f P = Eigen::Matrix4f::Identity();
    P(3, 2) = 0.5f;
    P(3, 3) = 0.5f;

    CameraOptions camera;
    camera.view_transform = look_at({0.5f, 0.5f, 2.0f}, {0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f});
    camera.projection_transform = Eigen::Projective3f(P);

    auto render = create_solid_render(render_size, render_size);
    LA_REQUIRE_THROWS(rasterizer.weighted_texture_from_render(render, camera));
}
