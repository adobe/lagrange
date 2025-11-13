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

#include <lagrange/texproc/TextureRasterizer.h>

#include "mesh_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Misha/Texels.h>
#include <Misha/SquaredEDT.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <atomic>
#include <functional>

/// @cond LA_INTERNAL_DOCS

namespace lagrange::texproc {

namespace {

// Internal camera parameters
//
// We're using terminology from the OpenGL coordinate systems
// https://learnopengl.com/Getting-started/Coordinate-Systems
struct CameraParameters
{
    unsigned int res[2];
    Eigen::Affine3d view_from_world = Eigen::Affine3d::Identity(); // world -> view
    Eigen::Projective3d ndc_from_view = Eigen::Projective3d::Identity(); // world -> ndc
    Eigen::Affine2d screen_from_ndc = Eigen::Affine2d::Identity(); // ndc -> screen

    CameraParameters(
        const Eigen::Affine3d& view_from_world_,
        const Eigen::Projective3d& ndc_from_view_,
        unsigned int width,
        unsigned int height)
    {
        res[0] = width;
        res[1] = height;
        view_from_world = view_from_world_;

        // Remap depth from [-1, 1] to [0, 1] to improve numerical precision
        // https://www.reedbeta.com/blog/depth-precision-visualized/
        ndc_from_view = Eigen::Scaling(1., 1., 0.5) * Eigen::Translation3d(0, 0, 1) *
                        Eigen::Scaling(1., 1., -1.) * ndc_from_view_;

        // https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/projection-matrix-GPU-rendering-pipeline-clipping.html
        //
        // x' = (x + 1) * 0.5 * (w - 1)
        // y' = (1 - (y+1) * 0.5) * (h - 1)
        const double w = static_cast<double>(width - 1);
        const double h = static_cast<double>(height - 1);
        screen_from_ndc =
            Eigen::Scaling(w / 2, h / 2) * Eigen::Translation2d(1, 1) * Eigen::Scaling(1., -1.);
    }

    Vector<double, 2> operator()(Vector<double, 3> p) const
    {
        auto p_ndc = world_to_ndc(p);
        Eigen::Vector2d p_screen = screen_from_ndc * Eigen::Vector2d(p_ndc[0], p_ndc[1]);
        return {p_screen[0], p_screen[1]};
    }

    Vector<double, 3> world_to_view(Vector<double, 3> p) const
    {
        Eigen::Vector3d p_world(p[0], p[1], p[2]);
        Eigen::Vector3d p_view = view_from_world * p_world;
        return {p_view[0], p_view[1], p_view[2]};
    }

    Vector<double, 3> world_to_ndc(Vector<double, 3> p) const
    {
        Eigen::Vector3d p_world(p[0], p[1], p[2]);
        Eigen::Vector3d p_ndc =
            (ndc_from_view * (view_from_world * p_world).homogeneous()).hnormalized();
        return {p_ndc[0], p_ndc[1], p_ndc[2]};
    }

    Vector<double, 3> camera_position_world() const
    {
        Eigen::Vector3d p_world = view_from_world.inverse() * Eigen::Vector3d::Zero();
        return {p_world[0], p_world[1], p_world[2]};
    }
};

// Internal class representing the rendering image and the camera that did the imaging
template <typename T>
struct Rendering
{
    CameraParameters camera_parameters;
    RegularGrid<2, T> render_map;

    Rendering(const CameraOptions& options, image::experimental::View3D<const float> rendered_image)
        : camera_parameters(
              options.view_transform.cast<double>(),
              options.projection_transform.cast<double>(),
              static_cast<unsigned int>(rendered_image.extent(0)),
              static_cast<unsigned int>(rendered_image.extent(1)))
    {
        mesh_utils::set_grid(rendered_image, render_map);
    }
};

// Functionality for computing the squared distance from a set of active texels
struct SquareDistanceToTextureBoundary
{
    SquareDistanceToTextureBoundary(const unsigned int res[K])
    {
        for (unsigned int k = 0; k < K; k++) {
            m_res[k] = res[k];
        }
    }

    template< typename ActiveTexelFunctor /* = std::function< bool ( typename RegularGrid< Dim >::Index ) > */ >
    RegularGrid<K, unsigned int> operator()(ActiveTexelFunctor&& F) const
    {
        static_assert(
            std::is_convertible_v<
                ActiveTexelFunctor,
                std::function<bool(typename RegularGrid<K>::Index)>>,
            "[ERROR] ActiveTexelFunctor poorly formed");

        using Index = typename RegularGrid<K>::Index;
        using Range = typename RegularGrid<K>::Range;

        Miscellany::PerformanceMeter pMeter;
        RegularGrid<K, unsigned int> d2(m_res);
        RegularGrid<K, unsigned char> raster(m_res);
        RegularGrid<K, unsigned char> boundary(m_res);

        Range range;
        for (unsigned int k = 0; k < K; k++) {
            range.second[k] = m_res[k];
        }

        // Identify all active texels
        range.processParallel([&](unsigned int, Index I) { raster(I) = F(I) ? 1 : 0; });

        // Identify the boundary texels
        {
            auto kernel = [&](unsigned int, Index I) {
                if (raster(I))
                    boundary(I) = 0;
                else {
                    bool has_active_neighbors = false;
                    Range::Intersect(range, Range(I).dilate(1)).process([&](Index i) {
                        has_active_neighbors |= raster(i) != 0;
                    });
                    boundary(I) = has_active_neighbors ? 1 : 0;
                }
            };
            range.processParallel(kernel);
        }

        d2 = SquaredEDT<double, K>::Saito(boundary);

        ThreadPool::ParallelFor(0, d2.size(), [&](unsigned int, size_t i) {
            if (!raster[i]) d2[i] = 0;
        });

        return d2;
    }

protected:
    unsigned int m_res[K];
};

// A class wrapping a depth image supporting the evaluation at non-integer positions
struct DepthMapWrapper
{
    DepthMapWrapper(const RegularGrid<2, double>& depth_map_)
        : depth_map(depth_map_)
    {}

    double operator()(Vector<double, 2> p) const
    {
        double v = 0;

        int ix = static_cast<int>(floor(p[0]));
        int iy = static_cast<int>(floor(p[1]));
        double dx[] = {1. - (p[0] - ix), p[0] - ix}, dy[] = {1.0 - (p[1] - iy), p[1] - iy};
        for (int ii = 0; ii < 2; ii++)
            for (int jj = 0; jj < 2; jj++) {
                if ((ix + ii) >= 0 && (ix + ii) < static_cast<int>(depth_map.res(0)) &&
                    (iy + jj) >= 0 && (iy + jj) < static_cast<int>(depth_map.res(1))) {
                    if (std::isfinite(depth_map(ix + ii, iy + jj))) {
                        v += depth_map(ix + ii, iy + jj) * dx[ii] * dy[jj];
                    } else {
                        return depth_map(ix + ii, iy + jj);
                    }
                }
            }

        return v;
    };

    const RegularGrid<2, double>& depth_map;
};

// Internal class representing the geometry
struct Mesh
{
    std::vector<Vector<double, 3>> vertices;
    std::vector<SimplexIndex<2>> triangles;
};

// A structure for computing texture images and confidences from renderings and camera parameters.
// The structure is initialized with the a 3D texture-mapped mesh
struct TextureAndConfidenceFromRender
{
    using MyTexels = Texels<true>;
    using MyTexelInfo = typename MyTexels::TexelInfo<2>;

    Mesh m_mesh;
    Padding m_padding;
    RegularGrid<2, MyTexelInfo> m_info_map;

    template <typename Scalar, typename Index>
    TextureAndConfidenceFromRender(
        const SurfaceMesh<Scalar, Index>& surface_mesh,
        unsigned int width,
        unsigned int height)
    {
        // Transform the mesh
        auto wrapper = mesh_utils::create_mesh_wrapper(
            surface_mesh,
            RequiresIndexedTexcoords::Yes,
            CheckFlippedUV::Yes);
        m_padding = mesh_utils::create_padding(wrapper, width, height);
        width += m_padding.width();
        height += m_padding.height();

        m_mesh.vertices.resize(wrapper.num_vertices());
        for (size_t v = 0; v < wrapper.num_vertices(); v++) {
            m_mesh.vertices[v] = wrapper.vertex(v);
        }
        m_mesh.triangles.resize(wrapper.num_simplices());
        for (size_t t = 0; t < wrapper.num_simplices(); t++) {
            for (unsigned int k = 0; k < 3; k++) {
                m_mesh.triangles[t][k] = wrapper.vertex_index(t, k);
            }
        }

        // Functionality returning the specified texture simplex
        auto get_texture_triangle = [&](size_t t) {
            Simplex<double, 2, 2> tri;
            for (unsigned int k = 0; k < 3; k++) {
                tri[k] =
                    wrapper.vflipped_texcoord(static_cast<size_t>(wrapper.texture_index(t, k)));
            }
            return tri;
        };

        // Compute the rasterization info [ONCE]
        unsigned int res[] = {width, height};
        m_info_map = MyTexels::template GetSupportedTexelInfo<3, false>(
            m_mesh.triangles.size(),
            [&](size_t v) { return m_mesh.vertices[v]; },
            [&](size_t t) { return m_mesh.triangles[t]; },
            get_texture_triangle,
            res,
            0,
            false);
    }

    // Computes the depth map associated to a rendering of the geometry using the prescribed
    // camera parameters and target resolution
    RegularGrid<2, double> compute_depth(const CameraParameters& camera_parameters) const
    {
        // Since we're sampling this as an unshifted RegularGrid, values are at the corners.

        RegularGrid<2, double> depth(camera_parameters.res);
        for (size_t i = 0; i < depth.size(); i++) {
            depth[i] = std::numeric_limits<double>::infinity();
        }

        typename RegularGrid<2>::Range range;
        for (unsigned int d = 0; d < 2; d++) {
            range.second[d] = camera_parameters.res[d];
        }

        Eigen::Projective3d view_from_ndc = camera_parameters.ndc_from_view.inverse();
        Eigen::Affine2d ndc_from_screen = camera_parameters.screen_from_ndc.inverse();

        for (unsigned int t = 0; t < m_mesh.triangles.size(); t++) {
            Simplex<double, 2, 2> t_tri;
            Simplex<double, 3, 2> c_tri;
            for (unsigned int k = 0; k < 3; k++) {
                t_tri[k] = camera_parameters(m_mesh.vertices[m_mesh.triangles[t][k]]);
                c_tri[k] = camera_parameters.world_to_view(m_mesh.vertices[m_mesh.triangles[t][k]]);
            }

            auto kernel = [&](typename RegularGrid<2>::Index I) {
                // TODO: Fix upstream rasterization code and make this if() an assert:
                // la_debug_assert(range.contains(I));
                if (!range.contains(I)) {
                    logger().debug(
                        "Index out of range in depth computation: ({}, {}) / ({}, {})",
                        I[0],
                        I[1],
                        range.second[0],
                        range.second[1]);
                    return;
                }
                constexpr bool NodeAtCellCenter = false;
                auto npos_screen = Texels<NodeAtCellCenter>::NodePosition(I);
                Eigen::Vector2d npos_ndc =
                    ndc_from_screen * Eigen::Vector2d(npos_screen[0], npos_screen[1]);
                const double zfar_ndc = 0.0;
                Eigen::Vector3d npos_view =
                    (view_from_ndc *
                     Eigen::Vector3d(npos_ndc[0], npos_ndc[1], zfar_ndc).homogeneous())
                        .hnormalized();
                Ray<double, 3> ray;
                ray.direction = Vector<double, 3>(npos_view[0], npos_view[1], npos_view[2]);
                std::pair<double, Vector<double, 3>> _bc = c_tri.barycentricCoordinates(ray);
                Vector<double, 3> bc = _bc.second;
                Vector<double, 3> p_c = c_tri[0] * bc[0] + c_tri[1] * bc[1] + c_tri[2] * bc[2];
                double d = -p_c[2];
                if (d > 0 && d < depth(I)) {
                    depth(I) = d;
                }
            };
            constexpr bool NodeAtCellCenter = false;
            Rasterizer2D::template RasterizeNodes<NodeAtCellCenter>(t_tri, kernel, range);
        }

        return depth;
    }

    // Computes the depth map discontinuity
    static RegularGrid<2, double> compute_depth_discontinuity(const RegularGrid<2, double>& depth)
    {
        RegularGrid<K, double> depth_discontinuity(depth.res());

        {
            typename RegularGrid<K>::Range range;
            for (unsigned int k = 0; k < K; k++) range.second[k] = depth.res(k);

            // Compute discrete Laplacian
            auto Kernel = [&](unsigned int, typename RegularGrid<K>::Index I) {
                typename RegularGrid<K>::Range nbrs = typename RegularGrid<K>::Range(I).dilate(1);

                unsigned int count = 0;
                double value = 0;
                RegularGrid<K>::Range::Intersect(nbrs, range)
                    .process([&](typename RegularGrid<2>::Index J) {
                        if (I != J) {
                            count++, value += depth(J);
                        }
                    });
                depth_discontinuity(I) = fabs(depth(I) - value / count);
            };
            range.processParallel(Kernel);

#if 0 // Requires C++20
            std::atomic<double> var = 0;
            std::atomic<size_t> count = 0;
            ThreadPool::ParallelFor(0, depth_discontinuity.size(), [&](size_t i) {
                if (depth_discontinuity[i] < std::numeric_limits<double>::infinity()) {
                    count++;
                    var += depth_discontinuity[i] * depth_discontinuity[i];
                }
            });
#else // Supported by C++17 (using map/reduce)
            std::vector<double> vars(ThreadPool::NumThreads(), 0);
            std::atomic<size_t> count = 0;
            ThreadPool::ParallelFor(0, depth_discontinuity.size(), [&](unsigned int t, size_t i) {
                if (depth_discontinuity[i] < std::numeric_limits<double>::infinity()) {
                    count++;
                    vars[t] += depth_discontinuity[i] * depth_discontinuity[i];
                }
            });
            double var = 0;
            for (unsigned int i = 0; i < vars.size(); i++) var += vars[i];
#endif
            double dev = sqrt(var / count);

            ThreadPool::ParallelFor(0, depth_discontinuity.size(), [&](size_t i) {
                depth_discontinuity[i] /= dev;
            });
        }

        return depth_discontinuity;
    }

    // Computes the confidence defined by a depth map
    // -- The depth discontinuities are identified and a confidence map is defined by having values
    //    fall off from one to zero as discontinuities are approached.
    // -- The radius of the fall off is given by the depth_discontinuity_erosion_radius parameter.
    RegularGrid<2, double> depth_confidence(
        const RegularGrid<2, double>& depth,
        double depth_discontinuity_erosion_radius,
        double depth_discontinuity_threshold) const
    {
        // Compute the depth discontinuity
        RegularGrid<K, double> depth_discontinuity = compute_depth_discontinuity(depth);

        // Compute the confidence form the depth
        RegularGrid<K, double> depth_confidence(depth.res());

        {
            unsigned int radius2 =
                depth_discontinuity_erosion_radius * depth_discontinuity_erosion_radius;

            SquareDistanceToTextureBoundary d2b(depth.res());
            RegularGrid<K, unsigned int> d2 = d2b([&](typename RegularGrid<K>::Index I) {
                return depth_discontinuity(I) < depth_discontinuity_threshold;
            });
            ThreadPool::ParallelFor(0, depth_confidence.size(), [&](size_t i) {
                if (d2[i] < radius2) {
                    depth_confidence[i] = sqrt(static_cast<double>(d2[i]) / radius2);
                } else {
                    depth_confidence[i] = 1.;
                }
            });
        }
        return depth_confidence;
    }

    // Returns the world position of a texel, if it is active
    std::optional<Vector<double, 3>> texel_world_position(size_t i) const
    {
        // If the texel is assigned a mesh triangle
        if (m_info_map[i].sIdx != static_cast<size_t>(-1)) {
            // Compute the 3D triangle
            SimplexIndex<2> si = m_mesh.triangles[m_info_map[i].sIdx];
            Simplex<double, Dim, 2> s(
                m_mesh.vertices[si[0]],
                m_mesh.vertices[si[1]],
                m_mesh.vertices[si[2]]);
            // Return the barycentric interpolation of the corners
            // (Note that the barycentric weights may extrapolate if the texel is active but not
            // covered)
            return s(m_info_map[i].bc);
        } else {
            return {};
        }
    }

    // Returns the world position and normal of a texel, if it is active
    std::optional<std::pair<Vector<double, 3>, Vector<double, 3>>> texel_world_position_and_normal(
        size_t i) const
    {
        // If the texel is assigned a mesh triangle
        if (m_info_map[i].sIdx != static_cast<size_t>(-1)) {
            // Compute the 3D triangle
            SimplexIndex<2> si = m_mesh.triangles[m_info_map[i].sIdx];
            Simplex<double, Dim, 2> s(
                m_mesh.vertices[si[0]],
                m_mesh.vertices[si[1]],
                m_mesh.vertices[si[2]]);
            // Return the barycentric interpolation of the corners
            // (Note that the barycentric weights may extrapolate if the texel is active but not
            // covered)
            return std::make_pair(s(m_info_map[i].bc), s.normal());
        } else {
            return {};
        }
    }

    // Computes the texture image by back-projecting the rendering into the texture image
    template <typename T>
    RegularGrid<2, T> texture_image(const Rendering<T>& rendering) const
    {
        RegularGrid<2, T> texture(m_info_map.res());

        // Iterate through the pixels of the texture (in parallel)
        ThreadPool::ParallelFor(0, m_info_map.size(), [&](size_t i) {
            if (auto world_texel = texel_world_position(i)) {
                Vector<double, 2> q = rendering.camera_parameters(*world_texel);
                texture[i] = rendering.render_map(q);
            }
        });

        m_padding.unpad(texture);
        return texture;
    }

    // Computes the texture confidence  using a combination of:
    // -- visibility,
    // -- proximity to depth discontinuities, and
    // -- normal alignment
    RegularGrid<2, double> texture_confidence(
        const CameraParameters& camera_params,
        unsigned int depth_discontinuity_erosion_radius,
        double depth_discontinuity_threshold,
        double depth_precision) const
    {
        // Resize
        RegularGrid<K, double> confidence(m_info_map.res());

        // Initialize the confidence to zero
        for (size_t i = 0; i < confidence.size(); i++) {
            confidence[i] = 0;
        }

        typename RegularGrid<K>::Range range;
        for (unsigned int k = 0; k < 2; k++) range.second[k] = camera_params.res[k];

        // Compute the depth
        RegularGrid<K, double> depth = this->compute_depth(camera_params);

        // Compute the depth-confidence
        RegularGrid<K, double> depth_confidence = this->depth_confidence(
            depth,
            depth_discontinuity_erosion_radius,
            depth_discontinuity_threshold);

        // Set the cumulative confidence based on the depth and normal confidence information
        {
            Vector<double, 3> camera_position = camera_params.camera_position_world();

            DepthMapWrapper depth_map(depth);

            // Iterate through the pixels of the texture (in parallel)
            ThreadPool::ParallelFor(0, m_info_map.size(), [&](size_t i) {
                if (auto world_texel = texel_world_position_and_normal(i)) {
                    // The position of the texel in world coordinates
                    Vector<double, Dim> p_w = world_texel->first;

                    // The normal of the texel in world coordinates
                    Vector<double, Dim> n = world_texel->second;
                    n /= Vector<double, Dim>::Length(n);

                    // The direction from the camera to the world position of the texel
                    Vector<double, Dim> dir = p_w - camera_position;
                    dir /= Vector<double, Dim>::Length(dir);

                    // The projection of the texl in the rendering
                    Vector<double, K> q = camera_params(p_w);

                    // The position of the texel in the camera coordinate system
                    Vector<double, Dim> p_c = camera_params.world_to_view(p_w);
                    const double z = -p_c[2]; // depth is -z in view space

                    // The depth map at the texel
                    double d = depth_map(q);

                    // The normal confidence based on the alignment of the normal with the camera's
                    // view direction
                    [[maybe_unused]] double normal_confidence =
                        fabs(Vector<double, Dim>::Dot(n, dir));

                    // If the texel is visible, set the confidence to the product of the depth and
                    // normal confidences
                    if (d < std::numeric_limits<double>::infinity() &&
                        z < d * (1. + depth_precision)) {
                        confidence[i] = normal_confidence * depth_confidence(q);
                    }
                }
            });
        }

        m_padding.unpad(confidence);
        return confidence;
    }
};

template <unsigned int NumChannels>
std::pair<image::experimental::Array3D<float>, image::experimental::Array3D<float>>
weighted_texture_from_render_impl(
    TextureAndConfidenceFromRender from_render,
    image::experimental::View3D<const float> rendered_image,
    const CameraOptions& camera_options,
    const TextureRasterizerOptions& rasterizer_options)
{
    Rendering<Vector<double, NumChannels>> in_rendering(camera_options, rendered_image);
    RegularGrid<2, Vector<double, NumChannels>> texture;
    RegularGrid<2, double> confidence;

    // Compute the texture image by back-projecting the renering
    texture = from_render.texture_image(in_rendering);

    // Compute the texture confidence from the visibility, normals, and depth discontinuity
    confidence = from_render.texture_confidence(
        in_rendering.camera_parameters,
        rasterizer_options.depth_discontinuity_erosion_radius,
        rasterizer_options.depth_discontinuity_threshold,
        rasterizer_options.depth_precision);

    // Set all zero-confidence texels to black
    for (unsigned int i = 0; i < confidence.size(); i++) {
        if (!confidence[i]) {
            texture[i] *= 0.;
        }
    }

    // Convert from internal to external
    auto texture_img =
        image::experimental::create_image<float>(texture.res(0), texture.res(1), NumChannels);
    auto confidence_img =
        image::experimental::create_image<float>(confidence.res(0), confidence.res(1), 1);

    mesh_utils::set_raw_view<NumChannels, float>(texture, texture_img);
    mesh_utils::set_raw_view<float>(confidence, confidence_img);

    return {texture_img, confidence_img};
}

} // namespace

template <typename Scalar, typename Index>
struct TextureRasterizer<Scalar, Index>::Impl
{
    TextureAndConfidenceFromRender from_render;
    TextureRasterizerOptions options;
};

template <typename Scalar, typename Index>
TextureRasterizer<Scalar, Index>::TextureRasterizer(
    const SurfaceMesh<Scalar, Index>& mesh,
    const TextureRasterizerOptions& options)
    : m_impl(
          make_value_ptr<Impl>(Impl{
              TextureAndConfidenceFromRender(
                  mesh,
                  static_cast<unsigned int>(options.width),
                  static_cast<unsigned int>(options.height)),
              options}))
{}

template <typename Scalar, typename Index>
TextureRasterizer<Scalar, Index>::~TextureRasterizer() = default;

template <typename Scalar, typename Index>
auto TextureRasterizer<Scalar, Index>::weighted_texture_from_render(
    image::experimental::View3D<const float> image,
    const CameraOptions& options) const -> std::pair<Array3Df, Array3Df>
{
    unsigned int num_channels = static_cast<unsigned int>(image.extent(2));
    switch (num_channels) {
    case 1:
        return weighted_texture_from_render_impl<1>(
            m_impl->from_render,
            image,
            options,
            m_impl->options);
    case 2:
        return weighted_texture_from_render_impl<2>(
            m_impl->from_render,
            image,
            options,
            m_impl->options);
    case 3:
        return weighted_texture_from_render_impl<3>(
            m_impl->from_render,
            image,
            options,
            m_impl->options);
    case 4:
        return weighted_texture_from_render_impl<4>(
            m_impl->from_render,
            image,
            options,
            m_impl->options);
    default:
        throw Error(
            fmt::format(
                "Only 1, 2, 3, or 4 channels supported. Input render image has {} channels.",
                num_channels));
    }
}

void filter_low_confidences(
    span<std::pair<image::experimental::Array3D<float>, image::experimental::Array3D<float>>>
        textures_and_confidences,
    const float low_ratio_threshold_)
{
    const auto low_ratio_threshold = static_cast<double>(low_ratio_threshold_);

    // Note that we do not normalize confidence values here. The normalization is done during the
    // compositing stage.
    if (textures_and_confidences.empty()) return;
    la_debug_assert(!textures_and_confidences.empty());
    std::vector<double> confidences(textures_and_confidences.size());
    for (size_t j = 0; j < textures_and_confidences[0].second.extent(1); j++) {
        for (size_t i = 0; i < textures_and_confidences[0].second.extent(0); i++) {
            // Read all confidence values for this texel
            for (size_t k = 0; k < textures_and_confidences.size(); k++) {
                confidences[k] = static_cast<double>(textures_and_confidences[k].second(i, j, 0));
            }
            // Find max confidence and discard confidences below threshold
            double confidence_max = 0;
            for (const auto confidence : confidences) {
                confidence_max = std::max<double>(confidence_max, confidence);
            }
            if (confidence_max > 0) {
                const auto confidence_threshold = confidence_max * low_ratio_threshold;
                for (auto& confidence : confidences) {
                    if (confidence < confidence_threshold) {
                        confidence = 0;
                    }
                }
            }
            // Write back the confidence values
            for (size_t k = 0; k < textures_and_confidences.size(); ++k) {
                textures_and_confidences[k].second(i, j, 0) = static_cast<float>(confidences[k]);
            }
        }
    }
}

#define LA_X_rasterizer_class(_, Scalar, Index) template class TextureRasterizer<Scalar, Index>;
LA_SURFACE_MESH_X(rasterizer_class, 0)

} // namespace lagrange::texproc

/// @endcond
