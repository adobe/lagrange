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

#include <lagrange/ui/Color.h>
#include <lagrange/ui/MeshBuffer.h>
#include <lagrange/ui/MeshModelBase.h>

namespace lagrange {
namespace ui {

// Creates MeshBuffer from Model
class MeshBufferFactory
{
public:

    /// Uploads vertex attribute
    /// data is an attribute matrix indexed per the original mesh
    template <typename MatrixType>
    static bool upload_vertex_attribute(
        VertexBuffer& target_buffer, const MatrixType& data, const ProxyMesh& pm)
    {
        target_buffer.upload(pm.flatten_vertex_attribute(data));
        return true;
    }

    /// Uploads facet attribute
    /// data is an attribute matrix indexed per the original mesh
    template <typename MatrixType>
    static bool upload_facet_attribute(
        VertexBuffer& target_buffer, const MatrixType& data, const ProxyMesh& pm)
    {
        target_buffer.upload(pm.flatten_facet_attribute(data));
        return true;
    }

    /// Uploads edge attribute
    /// data is an attribute matrix indexed per the original mesh
    template <typename MatrixType>
    static bool upload_edge_attribute(
        VertexBuffer& target_buffer, const MatrixType& data, const ProxyMesh& pm)
    {
        target_buffer.upload(pm.flatten_edge_attribute(data));
        return true;
    }

    /// Uploads corner attribute 
    /// data is an attribute matrix indexed per the original mesh
    template <typename MatrixType>
    static bool upload_corner_attribute(
        VertexBuffer& target_buffer, const MatrixType& data, const ProxyMesh& pm)
    {
        target_buffer.upload(pm.flatten_corner_attribute(data));
        return true;
    }

    static bool update_selection_indices(const ProxyMesh& proxy,
        const ElementSelection& selection,
        bool persistent,
        MeshBuffer& target_buffer);
};

} // namespace ui
} // namespace lagrange
