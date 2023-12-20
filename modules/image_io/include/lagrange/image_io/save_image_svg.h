/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/utils/range.h>

#include <fstream>

namespace lagrange {
namespace image_io {

struct SVGSetting
{
    bool with_stroke = true; ///< Whether to stroke the edges.
    bool with_fill = true; ///< Whether to fill the facets.
    bool use_uv_mesh = false; ///< Whether to use UV coordinates or vertex coordinates.
    uint32_t stroke_color = 0x000000; ///< Stroke color.
    uint32_t fill_color = 0xEBFF8C; ///< Fill color.
    float scaling_factor = 1; ///< Uniform scaling factor.
    float stroke_width = 1; ///< Stroke width.
    float width = 0; ///< Image width.  Auto-compute if <=0.
    float height = 0; ///< Image height.  Auto-compute if <=0.
};

/**
 * Save a mesh as a SVG image.
 *
 * @param[in] filename   Output filename.
 * @param[in] vertices   A #V by 2 array of vertex coordinates. If `vertices`
 *                       has 3 columns, only the first 2 will be used.
 * @param[in] facets     A #F by 3 array representing facets.
 * @param[in] settings   The settings struct.
 */
template <typename DerivedV, typename DerivedF>
void save_image_svg(
    const fs::path& filename,
    const Eigen::MatrixBase<DerivedV>& vertices,
    const Eigen::MatrixBase<DerivedF>& facets,
    const SVGSetting& settings = {})
{
    using Scalar = typename DerivedV::Scalar;
    std::ofstream fout(filename.c_str());

    const auto num_facets = facets.rows();

    const Eigen::Matrix<Scalar, 1, 2> bbox_min = vertices.colwise().minCoeff().eval();
    const Eigen::Matrix<Scalar, 1, 2> bbox_max = vertices.colwise().maxCoeff().eval();

    std::string footer = "</svg>";

    // Default image size is based on bbox.
    float width = settings.width;
    float height = settings.height;
    if (settings.width <= 0) {
        width = float(bbox_max[0] - bbox_min[0]) * settings.scaling_factor;
    }
    if (settings.height <= 0) {
        height = float(bbox_max[1] - bbox_min[1]) * settings.scaling_factor;
    }

    fout << fmt::format(
                "<?xml version=\"1.0\" encoding=\"utf-8\"?> <svg version=\"1.1\" id=\"Layer_1\" "
                "xmlns=\"http://www.w3.org/2000/svg\" "
                "xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" "
                "y=\"0px\" viewBox=\"0 0 {} {}\" "
                "style=\"enable-background:new 0 0 {} {};\" "
                "xml:space=\"preserve\"> <style type=\"text/css\"> "
                ".st0{{fill:{}; stroke:{}; stroke-miterlimit:10; stroke-width:{}px; "
                "stroke-linejoin:\"round\";}} "
                "</style> ",
                width,
                height,
                width,
                height,
                settings.with_fill ? fmt::format("#{:06x}", settings.fill_color) : "none",
                settings.with_stroke ? fmt::format("#{:06x}", settings.stroke_color) : "none",
                settings.with_stroke ? settings.stroke_width : 0)
         << std::endl;

    for (auto i : range(num_facets)) {
        fout << "<polygon class=\"st0\" points=\""
             << (vertices(facets(i, 0), 0) - bbox_min[0]) * settings.scaling_factor << ","
             << (bbox_max[1] - vertices(facets(i, 0), 1)) * settings.scaling_factor << " "
             << (vertices(facets(i, 1), 0) - bbox_min[0]) * settings.scaling_factor << ","
             << (bbox_max[1] - vertices(facets(i, 1), 1)) * settings.scaling_factor << " "
             << (vertices(facets(i, 2), 0) - bbox_min[0]) * settings.scaling_factor << ","
             << (bbox_max[1] - vertices(facets(i, 2), 1)) * settings.scaling_factor << "\"/>"
             << std::endl;
    }

    fout << footer << std::endl;
    fout.close();
}

/**
 * Save a mesh as a SVG image.
 */
template <typename MeshType>
void save_image_svg(const fs::path& filename, const MeshType& mesh, const SVGSetting& settings = {})
{
    if (settings.use_uv_mesh) {
        const auto& uv = mesh.get_uv();
        const auto& uv_indices = mesh.get_uv_indices();
        save_image_svg(filename, uv, uv_indices, settings);
    } else {
        const auto& vertices = mesh.get_vertices();
        const auto& facets = mesh.get_facets();
        save_image_svg(filename, vertices, facets, settings);
    }
}

} // namespace image_io
} // namespace lagrange
