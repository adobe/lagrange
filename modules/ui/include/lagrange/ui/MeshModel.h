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
#include <lagrange/ui/MeshModelBase.h>
#include <lagrange/ui/Model.h>

#include <chrono>
#include <future>
#include <memory>
#include <unordered_set>

namespace lagrange {
namespace ui {

namespace util {

/// Transforms a single vertex using Affine3f transform
/// If VertexType is 2D, the z coordinate is padded with 0
template <typename VertexType>
VertexType transform_vertex(const VertexType& v, const Eigen::Affine3f& T)
{
    Eigen::Vector4f v_ = Eigen::Vector4f::Zero();
    for (auto i = 0; i < v.cols(); i++) v_(i) = float(v(i));
    v_(3) = 1.0f;

    const auto vt = (T * v_).head<3>();
    VertexType v_new;
    for (auto i = 0; i < v.cols(); i++) v_new(i) = vt(i);
    return v_new;
}

} // namespace util

template <typename _MeshType>
class MeshModel : public MeshModelBase
{
public:
    using MeshType = _MeshType;

    MeshModel(std::unique_ptr<MeshType>&& mesh, const std::string& name = "Unnamed")
        : MeshModel(lagrange::to_shared_ptr(std::move(mesh)), name)
    {}

    MeshModel(
        Resource<MeshBase> mesh,
        const std::string& name = "Unnamed",
        Resource<ProxyMesh> proxy = {})
        : MeshModelBase(name)
    {
        m_mesh = mesh;
        if (proxy) {
            set_proxy(proxy);
        } else {
            update_proxy_mesh();
        }
    }

    // TODO constructor taking resource instead
    MeshModel(std::shared_ptr<MeshType> mesh, const std::string& name = "Unnamed")
        : MeshModelBase(name)
    {
        m_mesh = Resource<MeshBase>::create(std::move(mesh));
        update_proxy_mesh();
    }

    const MeshType& get_mesh() const
    {
        return m_mesh.cast<MeshType>();
    }

    bool has_mesh() const override { return m_mesh; }

    std::shared_ptr<MeshType> export_mesh()
    {
        return std::static_pointer_cast<MeshType>(m_mesh.data()->data());
    }


    void import_mesh(std::shared_ptr<MeshType> mesh)
    {
        m_mesh = Resource<MeshBase>::create(std::move(mesh));
        trigger_change();
    }

    bool transform_selection(const Eigen::Affine3f& T) override
    {
        using VertexArray = typename MeshType::VertexArray;
        using VertexType = typename MeshType::VertexType;
        using Index = typename MeshType::Index;

        if (!has_mesh()) return false;

        if (get_selection().get_type() == SelectionElementType::EDGE &&
            !get_mesh().is_edge_data_initialized()) {
            lagrange::logger().warn("Cannot transform, edge data is not initalized.");
            return false;
        }

        auto mesh = export_mesh();
        VertexArray V;
        mesh->export_vertices(V);

        auto& sel = get_selection().get_persistent().get_selection();
        if (get_selection().get_type() == SelectionElementType::VERTEX) {
            for (auto i : sel) {
                V.row(i) = util::transform_vertex<VertexType>(V.row(i), T);
            }
        } else if (
            get_selection().get_type() == SelectionElementType::EDGE &&
            mesh->is_edge_data_initialized()) {
            std::unordered_set<Index> unique_vertex_indices;

            auto& E = mesh->get_edges();
            for (auto i : sel) {
                auto& e = E[i];
                unique_vertex_indices.insert(e.v1());
                unique_vertex_indices.insert(e.v2());
            }
            for (auto i : unique_vertex_indices) {
                V.row(i) = util::transform_vertex<VertexType>(V.row(i), T);
            }
        } else if (get_selection().get_type() == SelectionElementType::FACE) {
            std::unordered_set<Index> unique_vertex_indices;
            auto& F = mesh->get_facets();
            for (auto i : sel) {
                for (auto k = 0; k < F.cols(); k++) {
                    unique_vertex_indices.insert(F(i, k));
                }
            }
            for (auto i : unique_vertex_indices) {
                V.row(i) = util::transform_vertex<VertexType>(V.row(i), T);
            }
        }

        mesh->import_vertices(V);
        import_mesh(mesh);

        return true;
    }

    DataGUID get_data_guid() const override { return DataGUID(m_mesh.get()); };

protected:
    void trigger_change() override
    {
        update_proxy_mesh();
        Model::trigger_change();
    }

    bool update_proxy_mesh()
    {
        set_proxy(Resource<ProxyMesh>::create_deferred(m_mesh, (MeshType*)(nullptr)));
        return true;
    }

private:

    Resource<MeshBase> m_mesh;
};

} // namespace ui
} // namespace lagrange
