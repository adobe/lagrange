/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <cmath>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/read_triangle_mesh.h>
#include <igl/write_triangle_mesh.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/common.h>
#include <lagrange/Mesh.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/timing.h>
#include <lagrange/raycasting/create_ray_caster.h>

template <typename RayCasterPtr, typename Mesh>
void ray_cast(RayCasterPtr& caster, Mesh& /*mesh*/, size_t N)
{
    size_t hit_counter = 0, counter = 0;
    Eigen::Vector3d axis(1, 1, 1);
    axis.normalize();
    for (size_t i = 0; i <= N; i++) {
        Eigen::Matrix4d trans;
        trans.setIdentity();
        double angle = double(i) / double(N) * 2 * M_PI;
        trans.block(0, 0, 3, 3) = Eigen::AngleAxis<double>(angle, axis).toRotationMatrix();

        caster->update_transformation(0, 0, trans);

        Eigen::Vector3d origin;
        origin.setZero();
        for (size_t j = 0; j <= N; j++) {
            double theta = double(j) / double(N) * 2 * M_PI;
            for (size_t k = 0; k <= N; k++) {
                double phi = double(k) / double(N) * M_PI - 0.5 * M_PI;
                Eigen::Vector3d direction(cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi));

                size_t mesh_index;
                size_t instance_index;
                size_t facet_index;
                double ray_depth;
                Eigen::Vector3d bc;
                Eigen::Vector3d norm;
                bool hit = caster->cast(origin, direction, mesh_index, instance_index, facet_index, ray_depth, bc, norm);
                counter++;
                if (hit) {
                    hit_counter++;
                }
            }
        }
    }
    std::cout << hit_counter << "/" << counter << " hit rate." << std::endl;
}

template <typename RayCasterPtr, typename Mesh>
void occlusion_cast(RayCasterPtr& caster, Mesh& /*mesh*/, size_t N)
{
    size_t hit_counter = 0, counter = 0;
    // caster->add_mesh(mesh, Eigen::Matrix4d::Identity());
    Eigen::Vector3d axis(1, 1, 1);
    axis.normalize();
    for (size_t i = 0; i <= N; i++) {
        Eigen::Matrix4d trans;
        trans.setIdentity();
        double angle = double(i) / double(N) * 2 * M_PI;
        trans.block(0, 0, 3, 3) = Eigen::AngleAxis<double>(angle, axis).toRotationMatrix();

        caster->update_transformation(0, 0, trans);

        Eigen::Vector3d origin;
        origin.setZero();
        for (size_t j = 0; j <= N; j++) {
            double theta = double(j) / double(N) * 2 * M_PI;
            for (size_t k = 0; k <= N; k++) {
                double phi = double(k) / double(N) * M_PI - 0.5 * M_PI;
                Eigen::Vector3d direction(cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi));

                Eigen::Vector3d bc;
                bool hit = caster->cast(origin, direction);
                counter++;
                if (hit) {
                    hit_counter++;
                }
            }
        }
    }
    std::cout << hit_counter << "/" << counter << " hit rate." << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " mesh N" << std::endl;
        return -1;
    }

    lagrange::TriangleMesh3D::VertexArray vertices;
    lagrange::TriangleMesh3D::FacetArray facets;

    igl::read_triangle_mesh(argv[1], vertices, facets);
    auto mesh = lagrange::to_shared_ptr(lagrange::create_mesh(vertices, facets));

    auto dynamic_caster =
        lagrange::raycasting::create_ray_caster<double>(lagrange::raycasting::EMBREE_DYNAMIC);
    dynamic_caster->add_mesh(mesh, Eigen::Matrix4d::Identity());
    ray_cast(dynamic_caster, mesh, 1);

    const size_t N = atoi(argv[2]);
    lagrange::timestamp_type t0, t1, t2;
    lagrange::get_timestamp(&t0);
    ray_cast(dynamic_caster, mesh, N);
    lagrange::get_timestamp(&t1);
    occlusion_cast(dynamic_caster, mesh, N);
    lagrange::get_timestamp(&t2);

    double dynamic_time = lagrange::timestamp_diff_in_seconds(t0, t1);
    double occlusion_time = lagrange::timestamp_diff_in_seconds(t1, t2);

    std::cout << "dyamic timing: " << dynamic_time << std::endl;
    std::cout << "occlusion timing: " << occlusion_time << std::endl;

    return 0;
}
