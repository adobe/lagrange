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
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/image_io/save_image.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/utils/assert.h>

#include <CLI/CLI.hpp>
#include <cassert>
#include <vector>

template <typename MeshType>
std::unique_ptr<MeshType> extract_uv_mesh(const MeshType& mesh)
{
    using Scalar = typename MeshType::Scalar;

    auto uv_values = mesh.get_uv();
    auto uv_indices = mesh.get_uv_indices();

    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> embedded_uv(uv_values.rows(), 3);
    embedded_uv.template leftCols<2>() = uv_values;
    embedded_uv.col(2).setZero();

    return lagrange::create_mesh(std::move(embedded_uv), std::move(uv_indices));
}

template <typename MeshType>
std::vector<float> generate_uv_image_with_normal_as_color(MeshType& mesh, size_t image_size)
{
    using Scalar = typename MeshType::Scalar;

    auto normal2rgb = [&](Eigen::Matrix<Scalar, 1, 3> n) {
        return ((n.array() + 1) / 2).template cast<float>();
    };

    auto uv_mesh = lagrange::to_shared_ptr(extract_uv_mesh(mesh));

    auto ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
        lagrange::raycasting::RayCasterType::EMBREE_ROBUST);
    ray_caster->add_mesh(uv_mesh, Eigen::Matrix<Scalar, 4, 4>::Identity());

    typename MeshType::AttributeArray normal_values;
    typename MeshType::IndexArray normal_indices;
    if (mesh.has_indexed_attribute("normal")) {
        lagrange::logger().info("Using indexed normal.");
        auto normals = mesh.get_indexed_attribute("normal");
        normal_values = std::get<0>(normals);
        normal_indices = std::get<1>(normals);
    } else if (mesh.has_vertex_attribute("normal")) {
        lagrange::logger().info("Using vertex normal.");
        normal_values = mesh.get_vertex_attribute("normal");
        normal_indices = mesh.get_facets();
    } else if (mesh.has_corner_attribute("normal")) {
        lagrange::logger().info("Using corner normal.");
        normal_values = mesh.get_corner_attribute("normal");
        normal_indices.resize(mesh.get_num_facets(), mesh.get_vertex_per_facet());
        std::iota(normal_indices.data(), normal_indices.data() + normal_indices.size(), 0);
    } else {
        compute_normal(mesh, lagrange::internal::pi / 6);
        auto normals = mesh.get_indexed_attribute("normal");
        normal_values = std::get<0>(normals);
        normal_indices = std::get<1>(normals);
    }

    std::vector<float> color_data(image_size * image_size * 4, 0);
    Eigen::Matrix<Scalar, 3, 1> origin, direction, barycentric_coord, p;
    Eigen::Matrix<float, 3, 1> c;
    size_t mesh_id, facet_id;
    Scalar ray_depth;
    for (size_t i = 0; i < image_size; i++) {
        Scalar u = (Scalar)i / (Scalar)image_size;
        for (size_t j = 0; j < image_size; j++) {
            Scalar v = (Scalar)j / (Scalar)image_size;
            auto index = i * image_size + j;
            origin = {u, v, 0.1};
            direction = {0, 0, -1};
            bool hit =
                ray_caster
                    ->cast(origin, direction, mesh_id, facet_id, ray_depth, barycentric_coord);
            if (!hit) continue;

            p = normal_values.row(normal_indices(facet_id, 0)) * barycentric_coord[0] +
                normal_values.row(normal_indices(facet_id, 1)) * barycentric_coord[1] +
                normal_values.row(normal_indices(facet_id, 2)) * barycentric_coord[2];
            p.normalize();
            c = normal2rgb(p);
            color_data[index * 4] = c[0];
            color_data[index * 4 + 1] = c[1];
            color_data[index * 4 + 2] = c[2];
            color_data[index * 4 + 3] = 1.0;
        }
    }

    return color_data;
}

template <typename MeshType>
std::vector<float> generate_uv_image_with_xyz_as_color(const MeshType& mesh, size_t image_size)
{
    using Scalar = typename MeshType::Scalar;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    Eigen::Matrix<Scalar, 1, 3> bbox_min = vertices.colwise().minCoeff();
    Eigen::Matrix<Scalar, 1, 3> bbox_max = vertices.colwise().maxCoeff();

    auto xyz2rgb = [&](Eigen::Matrix<Scalar, 1, 3> xyz) {
        Eigen::Matrix<float, 1, 3> rgb =
            ((xyz - bbox_min).array() / (bbox_max - bbox_min).array()).template cast<float>();
        return rgb;
    };

    auto uv_mesh = lagrange::to_shared_ptr(extract_uv_mesh(mesh));

    auto ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
        lagrange::raycasting::RayCasterType::EMBREE_ROBUST);
    ray_caster->add_mesh(uv_mesh, Eigen::Matrix<Scalar, 4, 4>::Identity());

    std::vector<float> color_data(image_size * image_size * 4, 0);
    Eigen::Matrix<Scalar, 3, 1> origin, direction, barycentric_coord, p;
    Eigen::Matrix<float, 3, 1> c;
    size_t mesh_id, facet_id;
    Scalar ray_depth;
    for (size_t i = 0; i < image_size; i++) {
        Scalar u = (Scalar)i / (Scalar)image_size;
        for (size_t j = 0; j < image_size; j++) {
            Scalar v = (Scalar)j / (Scalar)image_size;
            auto index = i * image_size + j;
            origin = {u, v, 0.1};
            direction = {0, 0, -1};
            bool hit =
                ray_caster
                    ->cast(origin, direction, mesh_id, facet_id, ray_depth, barycentric_coord);
            if (!hit) continue;

            p = vertices.row(facets(facet_id, 0)) * barycentric_coord[0] +
                vertices.row(facets(facet_id, 1)) * barycentric_coord[1] +
                vertices.row(facets(facet_id, 2)) * barycentric_coord[2];
            c = xyz2rgb(p);
            color_data[index * 4] = c[0];
            color_data[index * 4 + 1] = c[1];
            color_data[index * 4 + 2] = c[2];
            color_data[index * 4 + 3] = 1.0;
        }
    }

    return color_data;
}

int main(int argc, char const* argv[])
{
    struct
    {
        int image_size;
        std::string mode;
        std::string input;
        std::string output;
    } args;

    CLI::App app{"Convert mesh into a UV image with XYZ as color"};
    app.add_option("-s,--size", args.image_size, "Image size.")->default_val(1024);
    app.add_option("-m,--mode", args.mode, "Color mode to use, choices are \"xyz\" and \"normal\"")
        ->default_val("xyz");
    app.add_option("input", args.input, "Input mesh.");
    app.add_option("output", args.output, "Output image.");
    CLI11_PARSE(app, argc, argv);

    using MeshType = lagrange::TriangleMesh3D;
    auto mesh = lagrange::io::load_mesh<MeshType>(args.input);
    la_runtime_assert(mesh->is_uv_initialized(), "Input mesh does not contain UV.");

    std::vector<float> color_data;

    if (args.mode == "xyz") {
        color_data = generate_uv_image_with_xyz_as_color(*mesh, args.image_size);
    } else if (args.mode == "normal") {
        color_data = generate_uv_image_with_normal_as_color(*mesh, args.image_size);
    } else {
        lagrange::logger().error("Unsupported mode: {}", args.mode);
        la_runtime_assert(false);
    }

    lagrange::image_io::save_image_exr(
        args.output,
        reinterpret_cast<unsigned char*>(color_data.data()),
        args.image_size,
        args.image_size,
        lagrange::image::ImagePrecision::float32,
        lagrange::image::ImageChannel::four);

    return 0;
}
