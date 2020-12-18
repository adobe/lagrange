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
#include <lagrange/Logger.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/utils/range.h>


namespace lagrange {
namespace ui {


bool MeshBufferFactory::update_selection_indices(
    const ProxyMesh& proxy,
    const ElementSelection& element_selection,
    bool persistent,
    MeshBuffer& target_buffer)
{
    const auto sub_buffer_id = persistent ? "_selected" : "_hovered";
    const auto& selection = persistent ? element_selection.get_persistent().get_selection()
                                       : element_selection.get_transient().get_selection();


    std::vector<unsigned int> indices;
    int per_element = 0;

    // Translate original indices to proxy indices
    if (element_selection.get_type() == SelectionElementType::FACE) {
        per_element = 3;
        indices.reserve(selection.size() * 2 * 3);
        for (auto orig_fid : selection) {
            const auto proxy_tris = proxy.polygon_triangles(orig_fid);

            // Because proxy triangles are flattened, used their ids directly
            for (auto proxy_index : proxy_tris) {
                indices.push_back(3 * proxy_index + 0);
                indices.push_back(3 * proxy_index + 1);
                indices.push_back(3 * proxy_index + 2);
            }
        }
    } else if (element_selection.get_type() == SelectionElementType::VERTEX) {
        const auto& v2v_mapping = proxy.get_vertex_to_vertex_mapping();
        per_element = 1;
        indices.reserve(selection.size());

        for (auto orig_vid : selection) {
            indices.push_back(v2v_mapping[orig_vid]);
        }
    } else if (element_selection.get_type() == SelectionElementType::EDGE) {
        const auto& e2v = proxy.get_edge_to_vertices();
        per_element = 2;
        indices.reserve(selection.size() * 2);
        for (auto orig_eid : selection) {
            indices.push_back(e2v[2 * orig_eid + 0]);
            indices.push_back(e2v[2 * orig_eid + 1]);
        }
    } else {
        return false;
    }


    // Upload to gpu
    auto& index_buffer =
        target_buffer.get_sub_buffer(MeshBuffer::SubBufferType::INDICES, sub_buffer_id);

    index_buffer.upload(indices, per_element);
    return true;
}


} // namespace ui
} // namespace lagrange
