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
#pragma once

#include <lagrange/fs/filesystem.h>

#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/readPLY.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string>
#include <vector>

namespace lagrange {
namespace io {

/// Load .ply with color information (if available)
template <typename MeshType>
std::unique_ptr<MeshType> load_mesh_ply(const fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;

    VertexArray V;
    FacetArray F;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic> E;
    AttributeArray N;
    AttributeArray UV;

    AttributeArray VD;
    std::vector<std::string> VDheader;

    AttributeArray FD;
    std::vector<std::string> FDheader;

    AttributeArray ED;
    std::vector<std::string> EDheader;
    std::vector<std::string> comments;

    // TODO: libigl's wrapper around tinyply doesn't allow to load attributes of different tyeps
    // (e.g. char for colors + float for normals).
    igl::readPLY(
        filename.string(),
        V,
        F,
        E,
        N,
        UV,
        VD,
        VDheader,
        FD,
        FDheader,
        ED,
        EDheader,
        comments);

    auto mesh = create_mesh(std::move(V), std::move(F));
    if (N.rows() == mesh->get_num_vertices()) {
        logger().debug("Setting vertex normal");
        mesh->add_vertex_attribute("normal");
        mesh->import_vertex_attribute("normal", N);
    }

    auto has_attribute = [&](const std::string& name) {
        return std::find(VDheader.begin(), VDheader.end(), name) != VDheader.end();
    };

    if (VD.rows() == mesh->get_num_vertices()) {
        if (has_attribute("red") && has_attribute("green") && has_attribute("blue")) {
            bool has_alpha = has_attribute("alpha");
            int n = (has_alpha ? 4 : 3);
            AttributeArray color(mesh->get_num_vertices(), n);
            color.setZero();
            for (size_t i = 0; i < VDheader.size(); ++i) {
                if (VDheader[i] == "red") {
                    LA_ASSERT(n >= 1);
                    color.col(0) = VD.col(i);
                } else if (VDheader[i] == "green") {
                    LA_ASSERT(n >= 2);
                    color.col(1) = VD.col(i);
                } else if (VDheader[i] == "blue") {
                    LA_ASSERT(n >= 3);
                    color.col(2) = VD.col(i);
                } else if (VDheader[i] == "alpha") {
                    LA_ASSERT(n >= 4);
                    color.col(3) = VD.col(i);
                } else {
                    logger().warn("Unknown vertex attribute: {}", VDheader[i]);
                }
            }
            logger().debug("Setting vertex color");
            mesh->add_vertex_attribute("color");
            mesh->import_vertex_attribute("color", color);
        }
    }

    return mesh;
}

} // namespace io
} // namespace lagrange
