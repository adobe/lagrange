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
#include <lagrange/common.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
namespace raycasting {


class RaycasterMesh
{
public:
    using Index = size_t;

    virtual ~RaycasterMesh() {}

    virtual Index get_dim() const = 0;
    virtual Index get_vertex_per_facet() const = 0;
    virtual Index get_num_vertices() const = 0;
    virtual Index get_num_facets() const = 0;

    virtual std::vector<float> vertices_to_float() const = 0;
    virtual std::vector<unsigned> indices_to_int() const = 0;

    virtual void vertices_to_float(float* buf) const = 0; // buf must be preallocated
    virtual void indices_to_int(unsigned* buf) const = 0; // buf must be preallocated
};


template <typename MeshType>
class RaycasterMeshDerived : public RaycasterMesh
{
public:
    using Parent = RaycasterMesh;
    using Index = Parent::Index;

    RaycasterMeshDerived(std::shared_ptr<MeshType> mesh)
        : m_mesh(std::move(mesh))
    {
        la_runtime_assert(m_mesh, "Mesh cannot be null");
    }

    std::shared_ptr<MeshType> get_mesh_ptr() const { return m_mesh; }

    Index get_dim() const override { return m_mesh->get_dim(); }
    Index get_vertex_per_facet() const override { return m_mesh->get_vertex_per_facet(); }
    Index get_num_vertices() const override { return m_mesh->get_num_vertices(); };
    Index get_num_facets() const override { return m_mesh->get_num_facets(); };

    std::vector<float> vertices_to_float() const override
    {
        auto& data = m_mesh->get_vertices();
        const Index rows = safe_cast<Index>(data.rows());
        const Index cols = (get_dim() == 3 ? safe_cast<Index>(data.cols()) : 3);
        const Index size = rows * cols;

        // Due to Embree bug, we have to reserve space for one extra entry.
        // See https://github.com/embree/embree/issues/124
        std::vector<float> float_data;
        float_data.reserve(size + 1);
        float_data.resize(size);
        vertices_to_float(float_data.data());

        return float_data;
    }

    std::vector<unsigned> indices_to_int() const override
    {
        auto& data = m_mesh->get_facets();
        const Index rows = safe_cast<Index>(data.rows());
        const Index cols = safe_cast<Index>(data.cols());
        const Index size = rows * cols;

        // Due to Embree bug, we have to reserve space for one extra entry.
        // See https://github.com/embree/embree/issues/124
        std::vector<unsigned> int_data;
        int_data.reserve(size + 1);
        int_data.resize(size);
        indices_to_int(int_data.data());

        return int_data;
    }

    void vertices_to_float(float* buf) const override
    {
        auto& data = m_mesh->get_vertices();
        const Index rows = safe_cast<Index>(data.rows());

        if (get_dim() == 3) {
            const Index cols = safe_cast<Index>(data.cols());
            for (Index i = 0; i < rows; ++i) {
                for (Index j = 0; j < cols; ++j) {
                    *(buf++) = safe_cast<float>(data(i, j));
                }
            }
        } else if (get_dim() == 2) {
            for (Index i = 0; i < rows; ++i) {
                for (Index j = 0; j < 2; ++j) {
                    *(buf++) = safe_cast<float>(data(i, j));
                }
                *(buf++) = 0.0f;
            }
        }
    }

    void indices_to_int(unsigned* buf) const override
    {
        auto& data = m_mesh->get_facets();
        const Index rows = safe_cast<Index>(data.rows());
        const Index cols = safe_cast<Index>(data.cols());
        for (Index i = 0; i < rows; ++i) {
            for (Index j = 0; j < cols; ++j) {
                *(buf++) = safe_cast<unsigned>(data(i, j));
            }
        }
    }

    std::shared_ptr<MeshType> m_mesh;
};

} // namespace raycasting
} // namespace lagrange
