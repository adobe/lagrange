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

#include <lagrange/ui/MeshModelBase.h>


namespace lagrange {
namespace ui {

MeshModelBase::MeshModelBase(const std::string& name)
    : Model(name)
{}


void MeshModelBase::set_proxy(Resource<ProxyMesh> proxy)
{
    m_proxy = proxy;
    m_buffer = Resource<MeshBuffer>::create(proxy);
}

const ProxyMesh& MeshModelBase::get_proxy_mesh() const
{
    return *m_proxy;
}


AABB MeshModelBase::get_bounds() const
{
    auto proxy_bounds = m_proxy->get_bounds();
    return proxy_bounds.transformed(get_transform());
}

bool MeshModelBase::get_facet_at(
    Eigen::Vector3f origin,
    Eigen::Vector3f dir,
    int& facet_id_out,
    float& t_out,
    Eigen::Vector3f& barycentric_out)
{
    // Transform ray to proxy space
    auto invT = get_inverse_transform();
    origin = invT * origin;
    dir = invT.linear() * dir;

    return get_proxy_mesh()
        .get_original_facet_at(origin, dir, facet_id_out, t_out, barycentric_out);
}

bool MeshModelBase::get_facet_at(const Camera& cam, Eigen::Vector2f pixel, int& facet_id_out)
{
    bool is_inside;
    pixel = get_transformed_pixel(cam, pixel, is_inside);
    if (!is_inside) return false;

    auto ray = cam.cast_ray(pixel);

    float _t;
    Eigen::Vector3f _bary;
    return get_facet_at(ray.origin, ray.dir, facet_id_out, _t, _bary);
}

std::unordered_set<int> MeshModelBase::get_facets_in_frustum(
    const Frustum& f,
    bool ignore_backfacing) const
{
    return get_proxy_mesh().get_facets_in_frustum(
        f.transformed(get_inverse_transform()),
        ignore_backfacing,
        false);
}

std::unordered_set<int> MeshModelBase::get_facets_in_frustum(
    const Camera& cam,
    Eigen::Vector2f begin,
    Eigen::Vector2f end,
    bool ignore_backfacing) const
{
    bool visible;
    const auto f = get_transformed_frustum(cam, begin, end, visible);
    if (!visible) return {};

    return get_facets_in_frustum(f, ignore_backfacing);
}

std::unordered_set<int> MeshModelBase::get_vertices_in_frustum(
    const Frustum& f,
    bool ignore_backfacing) const
{
    return get_proxy_mesh().get_vertices_in_frustum(
        f.transformed(get_inverse_transform()),
        ignore_backfacing);
}

std::unordered_set<int> MeshModelBase::get_vertices_in_frustum(
    const Camera& cam,
    Eigen::Vector2f begin,
    Eigen::Vector2f end,
    bool ignore_backfacing) const
{
    bool visible;
    const auto f = get_transformed_frustum(cam, begin, end, visible);
    if (!visible) return {};

    return get_vertices_in_frustum(f, ignore_backfacing);
}

std::unordered_set<int> MeshModelBase::get_edges_in_frustum(
    const Frustum& f,
    bool ignore_backfacing) const
{
    return get_proxy_mesh().get_edges_in_frustum(
        f.transformed(get_inverse_transform()),
        ignore_backfacing);
}

std::unordered_set<int> MeshModelBase::get_edges_in_frustum(
    const Camera& cam,
    Eigen::Vector2f begin,
    Eigen::Vector2f end,
    bool ignore_backfacing) const
{
    bool visible;
    const auto f = get_transformed_frustum(cam, begin, end, visible);
    if (!visible) return {};

    return get_edges_in_frustum(f, ignore_backfacing);
}

bool MeshModelBase::get_vertex_at(
    const Camera& cam,
    Eigen::Vector2f pixel,
    float max_radius,
    int& vertex_id_out)
{
    auto& proxy = get_proxy_mesh();

    auto& vt = get_viewport_transform();
    max_radius *= 1.0f / std::sqrt(vt.scale.dot(vt.scale));

    bool is_inside;
    pixel = get_transformed_pixel(cam, pixel, is_inside);
    if (!is_inside) return false;

    auto ray = cam.cast_ray(pixel);
    auto dir = ray.dir;
    auto origin = ray.origin;

    auto invT = get_inverse_transform();
    origin = invT * origin;
    dir = invT.linear() * dir;

    float t;
    Eigen::Vector3f bary;
    int facet_id;
    bool isect = proxy.get_proxy_facet_at(origin, dir, facet_id, t, bary);
    if (!isect) return false;

    const auto& F = proxy.get_facets();
    const auto& V = proxy.get_vertices();

    int in_tri_index = 0;
    if (bary.x() > bary.y()) {
        in_tri_index = (bary.x() > bary.z()) ? 0 : 2;
    } else {
        in_tri_index = (bary.y() > bary.z()) ? 1 : 2;
    }

    int vertex_id = F(facet_id, in_tri_index);
    Eigen::Vector3f pos = get_transform() * V.row(vertex_id).transpose();
    auto screen_pos = cam.project(pos);
    screen_pos.y() = cam.get_window_height() - screen_pos.y();
    Eigen::Vector2f diff = screen_pos - pixel;

    if (diff.dot(diff) > max_radius * max_radius) return false;

    vertex_id_out = vertex_id;
    return true;
}

bool MeshModelBase::get_edge_at(
    const Camera& cam,
    Eigen::Vector2f pixel,
    float max_radius,
    int& edge_id_out)
{
    auto& proxy = get_proxy_mesh();

    auto& vt = get_viewport_transform();
    max_radius *= 1.0f / std::sqrt(vt.scale.dot(vt.scale));

    bool is_inside;
    pixel = get_transformed_pixel(cam, pixel, is_inside);
    if (!is_inside) return false;

    auto ray = cam.cast_ray(pixel);
    auto dir = ray.dir;
    auto origin = ray.origin;

    auto invT = get_inverse_transform();
    origin = invT * origin;
    dir = invT.linear() * dir;

    float t;
    Eigen::Vector3f bary;
    int facet_id;
    bool isect = proxy.get_proxy_facet_at(origin, dir, facet_id, t, bary);
    if (!isect) return false;

    const auto& F = proxy.get_facets();
    const auto& V = proxy.get_vertices();

    int in_tri_index = 0;
    if (bary.x() < bary.y()) {
        in_tri_index = (bary.x() < bary.z()) ? 0 : 2;
    } else {
        in_tri_index = (bary.y() < bary.z()) ? 1 : 2;
    }

    auto v0i = F(facet_id, (in_tri_index + 1) % 3);
    auto v1i = F(facet_id, (in_tri_index + 2) % 3);

    auto it = proxy.get_original_edge_index_map().find(ProxyMesh::Edge(v0i, v1i));
    if (it == proxy.get_original_edge_index_map().end()) return false;
    int edge_index = it->second;

    Eigen::Vector3f pos0 = get_transform() * V.row(v0i).transpose();
    Eigen::Vector3f pos1 = get_transform() * V.row(v1i).transpose();

    auto screen_pos0 = cam.project(pos0);
    screen_pos0.y() = cam.get_window_height() - screen_pos0.y();
    auto screen_pos1 = cam.project(pos1);
    screen_pos1.y() = cam.get_window_height() - screen_pos1.y();

    Eigen::Vector2f v = screen_pos1 - screen_pos0;
    Eigen::Vector2f w = pixel - screen_pos0;

    float c1 = w.dot(v);
    float c2 = v.dot(v);
    float b = c1 / c2;
    Eigen::Vector2f diff = (screen_pos0 + b * v) - pixel;

    if (diff.dot(diff) > max_radius * max_radius) return false;

    edge_id_out = edge_index;
    return true;
}


bool MeshModelBase::intersects(const Frustum& f)
{
    auto bb = get_bounds();

    // Broad phase
    if (!bb.intersects_frustum(f)) return false;

    return get_proxy_mesh().intersects(f.transformed(get_inverse_transform()));

    // return true;
}

bool MeshModelBase::intersects(
    const Eigen::Vector3f& ray_origin,
    const Eigen::Vector3f& ray_dir,
    float& t_out)
{
    auto bb = get_bounds();

    // Broad phase
    if (!bb.intersects_ray(ray_origin, ray_dir, &t_out)) return false;

    int fid;
    Eigen::Vector3f bary;
    return get_facet_at(ray_origin, ray_dir, fid, t_out, bary);
    // return true;
}

AABB MeshModelBase::get_selection_bounds() const
{
    return get_proxy_mesh().get_selection_bounds(get_selection()).transformed(get_transform());
}

void MeshModelBase::set_picking_enabled(bool value)
{
    m_proxy->set_picking_enabled(value);
}

bool MeshModelBase::is_picking_enabled() const
{
    return m_proxy->is_picking_enabled();
}

} // namespace ui
} // namespace lagrange
