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
#include <lagrange/Mesh.h>
#include <lagrange/ui/Model.h>
#include <lagrange/ui/ProxyMesh.h>
#include <lagrange/ui/Resource.h>
#include <lagrange/ui/MeshBuffer.h>

#include <atomic>
#include <future>
#include <unordered_set>

namespace lagrange {
namespace ui {


class MeshModelBase : public Model
{
public:
    MeshModelBase(const std::string& name);

    /// Sets new proxy resource
    /// Updates MeshBuffer
    void set_proxy(Resource<ProxyMesh> proxy);


    const ProxyMesh& get_proxy_mesh() const;

    AABB get_bounds() const override;

    /*
        World space raycasting
    */
    bool get_facet_at(
        Eigen::Vector3f origin,
        Eigen::Vector3f dir,
        int& facet_id_out,
        float& t_out,
        Eigen::Vector3f& barycentric_out);
    std::unordered_set<int> get_facets_in_frustum(const Frustum& f, bool ignore_backfacing) const;
    std::unordered_set<int> get_vertices_in_frustum(const Frustum& f, bool ignore_backfacing) const;
    std::unordered_set<int> get_edges_in_frustum(const Frustum& f, bool ignore_backfacing) const;
    bool intersects(const Frustum& f) override;
    bool intersects(const Eigen::Vector3f& ray_origin, const Eigen::Vector3f& ray_dir, float& t_out)
        override;

    /*
        Screen space raycasting (applies viewport transform)
    */
    bool get_facet_at(const Camera& cam, Eigen::Vector2f pixel, int& facet_id_out);
    bool
    get_vertex_at(const Camera& cam, Eigen::Vector2f pixel, float max_radius, int& vertex_id_out);
    bool get_edge_at(const Camera& cam, Eigen::Vector2f pixel, float max_radius, int& edge_id_out);

    std::unordered_set<int> get_facets_in_frustum(
        const Camera& cam,
        Eigen::Vector2f begin,
        Eigen::Vector2f end,
        bool ignore_backfacing) const;
    std::unordered_set<int> get_vertices_in_frustum(
        const Camera& cam,
        Eigen::Vector2f begin,
        Eigen::Vector2f end,
        bool ignore_backfacing) const;
    std::unordered_set<int> get_edges_in_frustum(
        const Camera& cam,
        Eigen::Vector2f begin,
        Eigen::Vector2f end,
        bool ignore_backfacing) const;

    /*
        Mesh can be exported/imported
        has_mesh returns false if it's in the exported state
    */
    virtual bool has_mesh() const = 0;


    AABB get_selection_bounds() const override;
    virtual bool transform_selection(const Eigen::Affine3f& T) = 0;

    Resource<MeshBuffer> get_buffer() const override { return m_buffer; }

protected:

    /// Triangle Proxy mesh for rendering, picking and bounds
    /// Depends on Resource<MeshBase>
    Resource<ProxyMesh> m_proxy;

    /// GPU Buffer containing sub buffers (vertices, indices, attributes, etc.)
    /// Depends on proxy
    Resource<MeshBuffer> m_buffer;

};


} // namespace ui
} // namespace lagrange
