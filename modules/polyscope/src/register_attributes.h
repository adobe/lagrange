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
#pragma once

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/point_cloud.h>
#include <polyscope/surface_mesh.h>
#include <polyscope/curve_network.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::polyscope {

inline bool show_as_vector(lagrange::AttributeUsage usage)
{
    // Attribute usages that can be shown as vectors in Polyscope
    switch (usage) {
    case lagrange::AttributeUsage::Vector: [[fallthrough]];
    case lagrange::AttributeUsage::Position: [[fallthrough]];
    case lagrange::AttributeUsage::Normal: [[fallthrough]];
    case lagrange::AttributeUsage::Tangent: [[fallthrough]];
    case lagrange::AttributeUsage::Bitangent: return true;
    default: return false;
    }
}

inline ::polyscope::VectorType vector_type(lagrange::AttributeUsage usage)
{
    switch (usage) {
    case lagrange::AttributeUsage::Vector: return ::polyscope::VectorType::AMBIENT;
    default: return ::polyscope::VectorType::STANDARD;
    }
}

// Convert integral color attributes to floating point
template <typename ValueType>
auto as_color_matrix(const lagrange::Attribute<ValueType>& attr) -> decltype(auto)
{
    return matrix_view(attr)
        .template cast<float>()
        // Int -> float conversion
        .unaryExpr([&](float x) { return std::is_floating_point_v<ValueType> ? x : x / 255.f; })
        // Gamma correction
        .unaryExpr([](float x) {
            float gamma = 2.2f;
            return std::pow(x, gamma);
        });
};

template <typename PolyscopeStructure>
struct PolyscopeTrait
{
    using PolyscopeType = std::decay_t<PolyscopeStructure>;
    static constexpr bool IsMesh = std::is_same_v<PolyscopeType, ::polyscope::SurfaceMesh>;
    static constexpr bool IsEdge = std::is_same_v<PolyscopeType, ::polyscope::CurveNetwork>;

    using QuantityType = std::conditional_t<
        IsMesh,
        ::polyscope::SurfaceMeshQuantity,
        std::conditional_t<
            IsEdge,
            ::polyscope::CurveNetworkQuantity,
            ::polyscope::PointCloudQuantity>>;
};

template <typename PolyscopeStructure>
using QuantityType = typename PolyscopeTrait<PolyscopeStructure>::QuantityType;

// Register a mesh attribute with Polyscope. Returns true if the attribute was registered.
template <typename PolyscopeStructure, typename ValueType>
auto register_attribute(
    PolyscopeStructure* ps_struct,
    std::string_view name_,
    const lagrange::Attribute<ValueType>& attr) -> QuantityType<PolyscopeStructure>*
{
    using Usage = lagrange::AttributeUsage;
    std::string name(name_); // TODO: Fix Polyscope's function signature to accept string_view

    using PolyscopeType = std::decay_t<PolyscopeStructure>;
    constexpr bool IsMesh = std::is_same_v<PolyscopeType, ::polyscope::SurfaceMesh>;
    constexpr bool IsEdge = std::is_same_v<PolyscopeType, ::polyscope::CurveNetwork>;

    switch (attr.get_element_type()) {
    case lagrange::AttributeElement::Vertex:
        if (attr.get_usage() == Usage::Scalar) {
            lagrange::logger().info("Registering scalar vertex attribute: {}", name);
            if constexpr (IsMesh) {
                return ps_struct->addVertexScalarQuantity(name, vector_view(attr));
            } else if constexpr (IsEdge) {
                return ps_struct->addNodeScalarQuantity(name, vector_view(attr));
            } else {
                return ps_struct->addScalarQuantity(name, vector_view(attr));
            }
        } else if (attr.get_num_channels() == 3) {
            if (show_as_vector(attr.get_usage())) {
                lagrange::logger().info("Registering vector vertex attribute: {}", name);
                ::polyscope::VectorType vt = vector_type(attr.get_usage());
                if constexpr (IsMesh) {
                    return ps_struct->addVertexVectorQuantity(name, matrix_view(attr), vt);
                } else if constexpr (IsEdge) {
                    return ps_struct->addNodeVectorQuantity(name, matrix_view(attr), vt);
                } else {
                    return ps_struct->addVectorQuantity(name, matrix_view(attr), vt);
                }
            } else if (attr.get_usage() == Usage::Color) {
                lagrange::logger().info("Registering color vertex attribute: {}", name);
                if constexpr (IsMesh) {
                    return ps_struct->addVertexColorQuantity(name, as_color_matrix(attr));
                } else if constexpr (IsEdge) {
                    return ps_struct->addNodeColorQuantity(name, as_color_matrix(attr));
                } else {
                    return ps_struct->addColorQuantity(name, as_color_matrix(attr));
                }
            }
        } else if (attr.get_num_channels() == 2) {
            if (show_as_vector(attr.get_usage())) {
                lagrange::logger().info("Registering 2D vector vertex attribute: {}", name);
                ::polyscope::VectorType vt = vector_type(attr.get_usage());
                if constexpr (IsMesh) {
                    return ps_struct->addVertexVectorQuantity2D(name, matrix_view(attr), vt);
                } else if constexpr (IsEdge) {
                    return ps_struct->addNodeVectorQuantity2D(name, matrix_view(attr), vt);
                } else {
                    return ps_struct->addVectorQuantity2D(name, matrix_view(attr), vt);
                }
            }
        } else if (attr.get_usage() == Usage::UV && attr.get_num_channels() == 2) {
            if constexpr (IsMesh) {
                lagrange::logger().info("Registering UV vertex attribute: {}", name);
                return ps_struct->addVertexParameterizationQuantity(name, matrix_view(attr));
            }
        }
        // TODO: If usage == Position, maybe add a toggle to switch between various vertex
        // positions
        break;
    case lagrange::AttributeElement::Facet:
        if (attr.get_usage() == Usage::Scalar) {
            lagrange::logger().info("Registering scalar facet attribute: {}", name);
            if constexpr (IsMesh) {
                return ps_struct->addFaceScalarQuantity(name, vector_view(attr));
            } else if constexpr (IsEdge) {
                return ps_struct->addEdgeScalarQuantity(name, vector_view(attr));
            }
        } else if (attr.get_num_channels() == 3) {
            if (show_as_vector(attr.get_usage())) {
                lagrange::logger().info("Registering vector facet attribute: {}", name);
                ::polyscope::VectorType vt = vector_type(attr.get_usage());
                if constexpr (IsMesh) {
                    return ps_struct->addFaceVectorQuantity(name, matrix_view(attr), vt);
                } else if constexpr (IsEdge) {
                    return ps_struct->addEdgeVectorQuantity(name, matrix_view(attr), vt);
                }
            } else if (attr.get_usage() == Usage::Color) {
                lagrange::logger().info("Registering color facet attribute: {}", name);
                if constexpr (IsMesh) {
                    return ps_struct->addFaceColorQuantity(name, as_color_matrix(attr));
                } else if constexpr (IsEdge) {
                    return ps_struct->addEdgeColorQuantity(name, as_color_matrix(attr));
                }
            }
        } else if (attr.get_num_channels() == 2) {
            if (show_as_vector(attr.get_usage())) {
                lagrange::logger().info("Registering vector facet attribute: {}", name);
                ::polyscope::VectorType vt = vector_type(attr.get_usage());
                if constexpr (IsMesh) {
                    return ps_struct->addFaceVectorQuantity2D(name, matrix_view(attr), vt);
                } else if constexpr (IsEdge) {
                    return ps_struct->addEdgeVectorQuantity2D(name, matrix_view(attr), vt);
                }
            }
        }
        break;
    case lagrange::AttributeElement::Edge:
        if constexpr (IsMesh) {
            if (attr.get_usage() == Usage::Scalar) {
                lagrange::logger().info("Registering scalar edge attribute: {}", name);
                return ps_struct->addEdgeScalarQuantity(name, vector_view(attr));
            }
        }
        break;
    case lagrange::AttributeElement::Corner:
        if constexpr (IsMesh) {
            if (attr.get_usage() == Usage::UV && attr.get_num_channels() == 2) {
                lagrange::logger().info("Registering UV corner attribute: {}", name);
                return ps_struct->addParameterizationQuantity(name, matrix_view(attr));
            }
        }
        break;
    default: break;
    }

    return nullptr;
}

template <typename PolyscopeStructure, typename Scalar, typename Index>
void register_attributes(PolyscopeStructure* ps_struct, const SurfaceMesh<Scalar, Index>& mesh)
{
    // Register mesh attributes supported by Polyscope
    seq_foreach_named_attribute_read(mesh, [&](auto name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (!AttributeType::IsIndexed) {
            if (!register_attribute(ps_struct, name, attr)) {
                lagrange::logger().warn("Skipping unsupported attribute: {}", name);
            }
        }
    });
}

} // namespace lagrange::polyscope
