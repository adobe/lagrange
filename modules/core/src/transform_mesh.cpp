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
            throw Error(format(
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
