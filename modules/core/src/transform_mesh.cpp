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
#include <lagrange/utils/compute_normal_cotransform.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
#include <lagrange/utils/fmt/format.h>
// clang-format on

namespace lagrange {

// Apply the geometric transform to one attribute's value matrix in place. Templated on
// <Scalar, Dimension, ValueType> only -- deliberately NOT on the mesh Index type. The heavy Eigen
// transform/cast machinery here is the dominant contributor to this TU's object size; keeping it
// Index-free means it is instantiated once per (Scalar, Dimension, ValueType) instead of being
// duplicated across every mesh index type. The row counter uses Eigen::Index for the same reason.
template <typename Scalar, int Dimension, typename ValueType>
void transform_attribute_values(
    RowMatrixView<ValueType> values,
    AttributeUsage usage,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const Eigen::Matrix<Scalar, Dimension, Dimension>& cotransform,
    const TransformOptions& options,
    bool is_reflection)
{
    // Select higher-precision type between Scalar and ValueType
    constexpr bool is_value_type_better = sizeof(ValueType) > sizeof(Scalar);
    using HigherPrecisionType = std::conditional_t<is_value_type_better, ValueType, Scalar>;

    auto A = transform.template cast<HigherPrecisionType>();
    auto L = transform.linear().template cast<HigherPrecisionType>();
    auto coL = cotransform.template cast<HigherPrecisionType>();
    auto X = values.template cast<HigherPrecisionType>().template leftCols<Dimension>().transpose();
    auto set = [&](auto&& Y) {
        values.template leftCols<Dimension>() = Y.transpose().template cast<ValueType>();
    };
    HigherPrecisionType sign(options.reorient && is_reflection ? -1 : 1);
    switch (usage) {
    case AttributeUsage::Position: set(A * X); break;
    case AttributeUsage::Normal:
        set(sign * coL * X);
        if (options.normalize_normals) {
            tbb::parallel_for(Eigen::Index(0), Eigen::Index(values.rows()), [&](Eigen::Index c) {
                values.row(c).template head<Dimension>().stableNormalize();
            });
        }
        break;
    case AttributeUsage::Tangent: [[fallthrough]];
    case AttributeUsage::Bitangent:
        set(sign * L * X);
        if (options.normalize_tangents_bitangents) {
            tbb::parallel_for(Eigen::Index(0), Eigen::Index(values.rows()), [&](Eigen::Index c) {
                values.row(c).template head<Dimension>().stableNormalize();
            });
        }
        break;
    default: break;
    }
}

template <typename Scalar, typename Index, int Dimension>
void transform_mesh_internal(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options,
    const BitField<AttributeUsage>& included_usages)
{
    la_runtime_assert(mesh.get_dimension() == Dimension, "Mesh dimension doesn't match transform");

    auto cotransform = compute_normal_cotransform(transform);

    bool is_reflection = (transform.linear().determinant() < 0);

    par_foreach_named_attribute_read(mesh, [&](auto&& name, auto&& attr_read) {
        using AttributeType = std::decay_t<decltype(attr_read)>;
        using ValueType = typename AttributeType::ValueType;

        const AttributeUsage usage = attr_read.get_usage();

        // Skip if we don't need to modify the attribute (to avoid triggering copy-on-write)
        switch (usage) {
        case AttributeUsage::Position:
        case AttributeUsage::Normal:
        case AttributeUsage::Tangent:
        case AttributeUsage::Bitangent: break;
        default: return;
        }

        // Filter by value type/indexed. The heavy Eigen transform lives in the Index-free
        // transform_attribute_values helper so it is not re-instantiated per mesh index type.
        if constexpr (std::is_floating_point_v<ValueType>) {
            // The included-usages check stays inside the floating-point branch so that a
            // non-floating attribute of a transformable usage still triggers the type error below,
            // matching the original behavior (the check used to live inside transform_values).
            if (!included_usages.test(usage)) {
                logger().debug("Skipping transform for attribute: {}", name);
                return;
            }
            if constexpr (AttributeType::IsIndexed) {
                auto& attr = mesh.template ref_indexed_attribute<ValueType>(name);
                transform_attribute_values<Scalar, Dimension>(
                    matrix_ref(attr.values()),
                    usage,
                    transform,
                    cotransform,
                    options,
                    is_reflection);
            } else {
                transform_attribute_values<Scalar, Dimension>(
                    attribute_matrix_ref<ValueType>(mesh, name),
                    usage,
                    transform,
                    cotransform,
                    options,
                    is_reflection);
            }
        } else {
            std::string_view type_name;
            if constexpr (AttributeType::IsIndexed) {
                type_name = internal::value_type_name(attr_read.values());
            } else {
                type_name = internal::value_type_name(attr_read);
            }
            throw Error(format(
                "Invalid attribute value type ({}) for attribute usage: {}",
                type_name,
                internal::to_string(usage)));
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
