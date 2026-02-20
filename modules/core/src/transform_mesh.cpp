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
#include <lagrange/transform_mesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

namespace {

template <typename Scalar>
Scalar sub(const Eigen::Matrix4<Scalar>& matrix, int i1, int i2, int i3, int j1, int j2, int j3)
{
    return matrix(i1, j1) * (matrix(i2, j2) * matrix(i3, j3) - matrix(i2, j3) * matrix(i3, j2));
}

template <int i, int j, typename Scalar>
inline Scalar minor4x4(const Eigen::Matrix4<Scalar>& matrix)
{
    int i1 = (i == 0 ? 1 : 0);
    int i2 = (i <= 1 ? 2 : 1);
    int i3 = (i == 3 ? 2 : 3);
    int j1 = (j == 0 ? 1 : 0);
    int j2 = (j <= 1 ? 2 : 1);
    int j3 = (j == 3 ? 2 : 3);
    return sub(matrix, i1, i2, i3, j1, j2, j3) + sub(matrix, i2, i3, i1, j1, j2, j3) +
           sub(matrix, i3, i1, i2, j1, j2, j3);
}

template <typename Scalar>
Eigen::Matrix4<Scalar> cofactor(const Eigen::Matrix4<Scalar>& matrix)
{
    Eigen::Matrix4<Scalar> result;
    result(0, 0) = minor4x4<0, 0>(matrix);
    result(0, 1) = -minor4x4<0, 1>(matrix);
    result(0, 2) = minor4x4<0, 2>(matrix);
    result(0, 3) = -minor4x4<0, 3>(matrix);
    result(2, 0) = minor4x4<2, 0>(matrix);
    result(2, 1) = -minor4x4<2, 1>(matrix);
    result(2, 2) = minor4x4<2, 2>(matrix);
    result(2, 3) = -minor4x4<2, 3>(matrix);
    result(1, 0) = -minor4x4<1, 0>(matrix);
    result(1, 1) = minor4x4<1, 1>(matrix);
    result(1, 2) = -minor4x4<1, 2>(matrix);
    result(1, 3) = minor4x4<1, 3>(matrix);
    result(3, 0) = -minor4x4<3, 0>(matrix);
    result(3, 1) = minor4x4<3, 1>(matrix);
    result(3, 2) = -minor4x4<3, 2>(matrix);
    result(3, 3) = minor4x4<3, 3>(matrix);
    return result;
}

template <typename Scalar>
Eigen::Matrix3<Scalar> compute_cotransform(
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform)
{
    return cofactor(transform.matrix()).template topLeftCorner<3, 3>();
}

template <typename Scalar>
Eigen::Matrix2<Scalar> compute_cotransform(
    const Eigen::Transform<Scalar, 2, Eigen::Affine>& transform)
{
    Eigen::Matrix4<Scalar> matrix = Eigen::Matrix4<Scalar>::Identity();
    matrix.template topLeftCorner<3, 3>() = transform.matrix().template topLeftCorner<3, 3>();
    return cofactor(matrix).template topLeftCorner<2, 2>();
}

} // namespace

template <typename Scalar, typename Index, int Dimension>
void transform_mesh_internal(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options,
    const BitField<AttributeUsage>& included_usages)
{
    la_runtime_assert(mesh.get_dimension() == Dimension, "Mesh dimension doesn't match transform");

    auto cotransform = compute_cotransform(transform);

    bool is_reflection = (transform.linear().determinant() < 0);

    par_foreach_named_attribute_read(mesh, [&](auto&& name, auto&& attr_read) {
        using AttributeType = std::decay_t<decltype(attr_read)>;
        using ValueType = typename AttributeType::ValueType;

        // Skip if we don't need to modify the attribute (to avoid triggering copy-on-write)
        switch (attr_read.get_usage()) {
        case AttributeUsage::Position:
        case AttributeUsage::Normal:
        case AttributeUsage::Tangent:
        case AttributeUsage::Bitangent: break;
        default: return;
        }

        // Select higher-precision type between Scalar and ValueType
        constexpr bool is_value_type_better = sizeof(ValueType) > sizeof(Scalar);
        using HigherPrecisionType = std::conditional_t<is_value_type_better, ValueType, Scalar>;

        // Apply geometric transform.
        auto transform_values = [&](auto&& values) {
            auto A = transform.template cast<HigherPrecisionType>();
            auto L = transform.linear().template cast<HigherPrecisionType>();
            auto coL = cotransform.template cast<HigherPrecisionType>();
            auto X = values.template cast<HigherPrecisionType>()
                         .template leftCols<Dimension>()
                         .transpose();
            auto set = [&](auto&& Y) {
                values.template leftCols<Dimension>() = Y.transpose().template cast<ValueType>();
            };
            HigherPrecisionType sign(options.reorient && is_reflection ? -1 : 1);
            if (!included_usages.test(attr_read.get_usage())) {
                logger().debug("Skipping transform for attribute: {}", name);
                return;
            }
            switch (attr_read.get_usage()) {
            case AttributeUsage::Position: set(A * X); break;
            case AttributeUsage::Normal:
                set(sign * coL * X);
                if (options.normalize_normals) {
                    tbb::parallel_for(Index(0), Index(values.rows()), [&](Index c) {
                        values.row(c).template head<3>().stableNormalize();
                    });
                }
                break;
            case AttributeUsage::Tangent: [[fallthrough]];
            case AttributeUsage::Bitangent:
                set(sign * L * X);
                if (options.normalize_tangents_bitangents) {
                    tbb::parallel_for(Index(0), Index(values.rows()), [&](Index c) {
                        values.row(c).template head<3>().stableNormalize();
                    });
                }
                break;
            default: break;
            }
        };

        // Filter by value type/indexed
        if constexpr (std::is_floating_point_v<ValueType>) {
            if constexpr (AttributeType::IsIndexed) {
                auto& attr = mesh.template ref_indexed_attribute<ValueType>(name);
                transform_values(matrix_ref(attr.values()));
            } else {
                transform_values(attribute_matrix_ref<ValueType>(mesh, name));
            }
        } else {
            LA_IGNORE(transform_values);
            std::string_view type_name;
            if constexpr (AttributeType::IsIndexed) {
                type_name = internal::value_type_name(attr_read.values());
            } else {
                type_name = internal::value_type_name(attr_read);
            }
            throw Error(
                fmt::format(
                    "Invalid attribute value type ({}) for attribute usage: {}",
                    type_name,
                    internal::to_string(attr_read.get_usage())));
        }
    });

    // We must flip facets due to transform with negative scale
    if (options.reorient && is_reflection) {
        // No need to have flip_facets reorient attributes again, since we already took care of that
        mesh.flip_facets([](Index /*f*/) { return true; });
    }
}

template <typename Scalar, typename Index, int Dimension>
void transform_mesh(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options)
{
    transform_mesh_internal(mesh, transform, options, BitField<AttributeUsage>::all());
}

template <typename Scalar, typename Index, int Dimension>
SurfaceMesh<Scalar, Index> transformed_mesh(
    SurfaceMesh<Scalar, Index> mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options)
{
    transform_mesh(mesh, transform, options);
    return mesh;
}

// TODO: Rely on LA_SIMPLE_SCENE_X to iterate over Dimension as well...
#define LA_X_transform_mesh(_, Scalar, Index)                                           \
    template LA_CORE_API void transform_mesh_internal(                                  \
        SurfaceMesh<Scalar, Index>& mesh,                                               \
        const Eigen::Transform<Scalar, 2, Eigen::Affine>& transform,                    \
        const TransformOptions& options,                                                \
        const BitField<AttributeUsage>& included_usages);                               \
    template LA_CORE_API void transform_mesh_internal(                                  \
        SurfaceMesh<Scalar, Index>& mesh,                                               \
        const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform,                    \
        const TransformOptions& options,                                                \
        const BitField<AttributeUsage>& included_usages);                               \
    template LA_CORE_API void transform_mesh<Scalar, Index, 2>(                         \
        SurfaceMesh<Scalar, Index> & mesh,                                              \
        const Eigen::Transform<Scalar, 2, Eigen::Affine>& transform,                    \
        const TransformOptions& options);                                               \
    template LA_CORE_API SurfaceMesh<Scalar, Index> transformed_mesh<Scalar, Index, 2>( \
        SurfaceMesh<Scalar, Index> mesh,                                                \
        const Eigen::Transform<Scalar, 2, Eigen::Affine>& transform,                    \
        const TransformOptions& options);                                               \
    template LA_CORE_API void transform_mesh<Scalar, Index, 3>(                         \
        SurfaceMesh<Scalar, Index> & mesh,                                              \
        const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform,                    \
        const TransformOptions& options);                                               \
    template LA_CORE_API SurfaceMesh<Scalar, Index> transformed_mesh<Scalar, Index, 3>( \
        SurfaceMesh<Scalar, Index> mesh,                                                \
        const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform,                    \
        const TransformOptions& options);
LA_SURFACE_MESH_X(transform_mesh, 0)

} // namespace lagrange
