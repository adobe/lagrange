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
#include <lagrange/io/types.h>

#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/safe_cast.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/writePLY.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string>
#include <vector>

namespace lagrange::io {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/// Save .ply with color information (if available)
template <
    typename MeshType,
    std::enable_if_t<lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* = nullptr>
void save_mesh_ply(
    const fs::path& filename,
    const MeshType& mesh,
    FileEncoding encoding = FileEncoding::Binary)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using VertexArray = typename MeshType::VertexArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using Index = unsigned int;

    const VertexArray& V = mesh.get_vertices();
    const auto F = mesh.get_facets().template cast<Index>();
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic> E;
    AttributeArray N;
    AttributeArray UV;

    Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic> VD;
    std::vector<std::string> VDheader;

    AttributeArray FD;
    std::vector<std::string> FDheader;

    AttributeArray ED;
    std::vector<std::string> EDheader;
    std::vector<std::string> comments;

    if (mesh.has_vertex_attribute("normal")) {
        N = mesh.get_vertex_attribute("normal");
    }

    if (mesh.has_vertex_attribute("color")) {
        VDheader = {"red", "green", "blue"};
        auto C = mesh.get_vertex_attribute("color");
        if (C.maxCoeff() <= 1.0 && C.maxCoeff() > 0.0) {
            logger().warn(
                "Max color value is > 0.0 but <= 1.0, but colors are saved as char. "
                "Please convert your colors to the range [0, 255].");
        }
        VD = C.template cast<unsigned char>();
        if (VD.cols() > 3) {
            VDheader.push_back("alpha");
        }
        la_runtime_assert(safe_cast<size_t>(VD.cols()) == VDheader.size());
    }

    igl::writePLY(
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
        comments,
        encoding == FileEncoding::Binary ? igl::FileEncoding::Binary : igl::FileEncoding::Ascii);
}

} // namespace legacy
} // namespace lagrange::io
