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
#include <lagrange/ui/VertexBuffer.h>
#include <lagrange/ui/ui_common.h>
#include <lagrange/utils/strings.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace lagrange {
namespace ui {


struct VertexBufferWrapper;

/*
    Manages GPU Vertex Buffer Objects for a single mesh.
    Can optionally contain additional SubBufferType buffers identified by SubBufferID, i.e.
    have different version identified by SubBufferID (e.g. different color buffers for
    different visualizations)
*/
class MeshBuffer {
public:

    enum class SubBufferType : uint8_t {
        POSITION = 0,
        NORMAL = 1,
        UV = 2,
        COLOR = 3,
        TANGENT = 4,
        BITANGENT = 5,
        _COUNT = 6,
        INDICES = 255
    };

    /*
        OpenGL primitive that gets rendered
    */
    enum class Primitive { POINTS, LINES, TRIANGLES };
    

    using SubBufferID = std::string;
    static SubBufferID default_sub_id();

    static SubBufferID vertex_index_id() { return "__default::vertices"; }
    static SubBufferID edge_index_id() { return "__default::edges"; }
    static SubBufferID facet_index_id() { return default_sub_id(); }
    static SubBufferID corner_index_id() { return default_sub_id(); }
    static SubBufferID material_index_id(int material_id)
    {
        return lagrange::string_format("material_indices_{}", material_id);
    }


    MeshBuffer(bool homogeneous = false);
    ~MeshBuffer();

    // Creates on demand if does not exist
    VertexBuffer& get_sub_buffer(SubBufferType subbuffer_type,
        const SubBufferID& id = MeshBuffer::default_sub_id());

    // Tries to get sub buffer with id, if that fails, it tries to get default one
    VertexBuffer* try_get_sub_buffer(SubBufferType subbuffer_type,
        const SubBufferID& id = MeshBuffer::default_sub_id());

    using SubBufferSelection = std::unordered_map<SubBufferType, std::string>;

    bool render(Primitive primitive,
        const SubBufferSelection & selection = {});

    size_t get_attribute_num() const;

    /*
        [-1,1]^2 Quad
    */
    static MeshBuffer& quad();

    /*
        Single point at [0,0,0]
    */
    static MeshBuffer& point();

    /*
        Returns an infinite plane mesh (w=0 for corners, w=1 for center)
    */
    static MeshBuffer& infinite_plane();

    /*
        [-1,1]^3 Cube
    */
    static MeshBuffer& cube(bool edges = false);

    //Explicit destructor for static data
    //Must be called before OpenGL shutdown
    static void clear_static_data() { 
        m_quad = nullptr;
        m_cube_triangles = nullptr;
        m_cube_edges = nullptr;
        m_point = nullptr;
    }
private:
    /*
        Try to find index buffer whose id != default_sub_id()
        E.g. for selection
    */
    VertexBuffer* non_default_index_buffer(const std::string& sub_id) const;
    using key_t = std::pair<SubBufferType, SubBufferID>;

    std::unordered_map<key_t, std::unique_ptr<VertexBufferWrapper>, utils::pair_hash> m_sub_buffers;
    VAO m_vao;
    bool m_homogeneous;

    static std::unique_ptr<MeshBuffer> m_quad;
    static std::unique_ptr<MeshBuffer> m_cube_triangles;
    static std::unique_ptr<MeshBuffer> m_cube_edges;
    static std::unique_ptr<MeshBuffer> m_point;
    static std::unique_ptr<MeshBuffer> m_infinite_plane;
};



} // namespace ui
} // namespace lagrange
