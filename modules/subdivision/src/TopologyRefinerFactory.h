/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
// No #pragma once at the top of this file, this is on purpose.

#include "visit_attribute.h"

#include <lagrange/Logger.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/utils/assert.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Far {

// Specify the number of vertices, faces, face-vertices, etc.
template <>
bool TopologyRefinerFactory<ConverterType>::resizeComponentTopology(
    TopologyRefiner& refiner,
    const ConverterType& conv)
{
    const auto& mesh = conv.mesh;

    // Number of vertices
    int num_vertices = static_cast<int>(mesh.get_num_vertices());
    setNumBaseVertices(refiner, num_vertices);

    // Number of faces and face-vertices (corners)
    int num_facets = static_cast<int>(mesh.get_num_facets());
    setNumBaseFaces(refiner, num_facets);
    for (int facet = 0; facet < num_facets; ++facet) {
        int nv = static_cast<int>(mesh.get_facet_size(facet));
        setNumBaseFaceVertices(refiner, facet, nv);
    }

    return true;
}

// Specify the relationships between vertices, faces, etc. ie the face-vertices, vertex-faces,
// edge-vertices, etc.
template <>
bool TopologyRefinerFactory<ConverterType>::assignComponentTopology(
    TopologyRefiner& refiner,
    const ConverterType& conv)
{
    const auto& mesh = conv.mesh;
    using Far::IndexArray;

    // Face relations
    int num_facets = static_cast<int>(mesh.get_num_facets());
    for (int facet = 0; facet < num_facets; ++facet) {
        IndexArray dst_facet_vertices = getBaseFaceVertices(refiner, facet);

        auto fv = mesh.get_facet_vertices(facet);
        for (size_t lv = 0; lv < fv.size(); ++lv) {
            dst_facet_vertices[lv] = static_cast<int>(fv[lv]);
        }
    }

    // If we have any non-manifold vertices/edges, we need to set their tags before calling this function
    populateBaseLocalIndices(refiner);

    return true;
};

// (Optional) Specify edge or vertex sharpness or face holes.
template <>
bool TopologyRefinerFactory<ConverterType>::assignComponentTags(
    TopologyRefiner& refiner,
    const ConverterType& conv)
{
    const auto& mesh = conv.mesh;
    const auto& options = conv.options;

    if (options.edge_sharpness_attr.has_value()) {
        lagrange::subdivision::visit_attribute(
            mesh,
            options.edge_sharpness_attr.value(),
            [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                la_runtime_assert(attr.get_num_channels() == 1);
                la_runtime_assert(
                    std::is_floating_point_v<ValueType>,
                    fmt::format(
                        "Edge sharpness attribute must use a floating point type. Received: {}",
                        lagrange::internal::value_type_name<ValueType>()));
                la_runtime_assert(attr.get_element_type() == lagrange::AttributeElement::Edge);
                if constexpr (AttributeType::IsIndexed) {
                    la_runtime_assert("Edge sharpness cannot be an indexed attribute");
                } else {
                    auto values = attr.get_all();
                    for (int e = 0; e < static_cast<int>(values.size()); ++e) {
                        auto v = mesh.get_edge_vertices(e);
                        Index idx = findBaseEdge(refiner, v[0], v[1]);

                        if (idx != INDEX_INVALID) {
                            float s = static_cast<float>(ValueType(10.0) * values[e]);
                            setBaseEdgeSharpness(refiner, idx, s);
                        } else {
                            char msg[1024];
                            snprintf(
                                msg,
                                1024,
                                "Edge %d specified to be sharp does not exist (%d, %d)",
                                e,
                                static_cast<int>(v[0]),
                                static_cast<int>(v[1]));
                            reportInvalidTopology(
                                Vtr::internal::Level::TOPOLOGY_INVALID_CREASE_EDGE,
                                msg,
                                conv);
                        }
                    }
                }
            });
    }

    if (options.vertex_sharpness_attr.has_value()) {
        lagrange::subdivision::visit_attribute(
            mesh,
            options.vertex_sharpness_attr.value(),
            [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                la_runtime_assert(attr.get_num_channels() == 1);
                la_runtime_assert(attr.get_element_type() == lagrange::AttributeElement::Vertex);
                la_runtime_assert(
                    std::is_floating_point_v<ValueType>,
                    fmt::format(
                        "Vertex sharpness attribute must use a floating point type. Received: {}",
                        lagrange::internal::value_type_name<ValueType>()));
                if constexpr (AttributeType::IsIndexed) {
                    la_runtime_assert("Vertex sharpness cannot be an indexed attribute");
                } else {
                    auto values = attr.get_all();
                    for (int v = 0; v < static_cast<int>(values.size()); ++v) {
                        float s = static_cast<float>(ValueType(10.0) * values[v]);
                        setBaseVertexSharpness(refiner, v, s);
                    }
                }
            });
    }

    if (options.face_hole_attr.has_value()) {
        lagrange::subdivision::visit_attribute(
            mesh,
            options.face_hole_attr.value(),
            [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                la_runtime_assert(attr.get_num_channels() == 1);
                la_runtime_assert(
                    std::is_integral_v<ValueType>,
                    fmt::format(
                        "Face holes attribute must use an integral type. Received: {}",
                        lagrange::internal::value_type_name<ValueType>()));
                if constexpr (AttributeType::IsIndexed) {
                    la_runtime_assert("Face holes cannot be an indexed attribute");
                } else {
                    auto values = attr.get_all();
                    for (int f = 0; f < static_cast<int>(values.size()); ++f) {
                        if (values[f] != ValueType(0)) {
                            lagrange::logger().warn("Setting facet f{} as a hole", f);
                            setBaseFaceHole(refiner, f, true);
                        }
                    }
                }
            });
    }

    return true;
}


// (Optional) Specify face-varying data per face.
template <>
bool TopologyRefinerFactory<ConverterType>::assignFaceVaryingTopology(
    TopologyRefiner& refiner,
    const ConverterType& conv)
{
    const auto& mesh = conv.mesh;

    // TODO: Only define one fvar channel for each different set of indices (factorize shared set of
    // indices)?
    for (lagrange::AttributeId attr_id : conv.face_varying_attributes) {
        lagrange::subdivision::visit_attribute(mesh, attr_id, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            if constexpr (!AttributeType::IsIndexed) {
                la_runtime_assert(
                    false,
                    fmt::format(
                        "Face varying attributes must indexed attributes. Received: {}",
                        lagrange::internal::to_string(attr.get_element_type())));
            } else {
                int num_values = static_cast<int>(attr.values().get_num_elements());
                auto src_indices = attr.indices().get_all();

                int channel = createBaseFVarChannel(refiner, num_values);

                for (int f = 0, src_next = 0; f < static_cast<int>(mesh.get_num_facets()); ++f) {
                    IndexArray dst_indices = getBaseFaceFVarValues(refiner, f, channel);

                    for (int lv = 0; lv < dst_indices.size(); ++lv) {
                        dst_indices[lv] = src_indices[src_next++];
                    }
                }
            }
        });
    }
    return true;
}

// (Optional) Control run-time topology validation and error reporting.
template <>
void TopologyRefinerFactory<ConverterType>::reportInvalidTopology(
    TopologyError /* errCode */,
    const char* msg,
    const ConverterType& /* mesh */)
{
    //
    //  Optional topology validation error reporting:
    //      This method is called whenever the factory encounters topology validation
    //  errors. By default, nothing is reported
    //
    lagrange::logger().warn("[opensubdiv] {}", msg);
}

} // namespace Far

} // namespace OPENSUBDIV_VERSION
} // namespace OpenSubdiv
