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

#include <memory>

#include <Eigen/Core>

#include <lagrange/MeshGeometry.h>
#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
template <typename _VertexArray, typename _FacetArray>
class GenuineMeshGeometry : public MeshGeometry<_VertexArray, _FacetArray>
{
public:
    using Parent = MeshGeometry<_VertexArray, _FacetArray>;
    using VertexArray = typename Parent::VertexArray;
    using FacetArray = typename Parent::FacetArray;
    using VertexType = typename Parent::VertexType;
    using Scalar = typename Parent::Scalar;
    using Index = typename Parent::Index;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    GenuineMeshGeometry() = default;
    GenuineMeshGeometry(const VertexArray& vertices, const FacetArray& facets)
        : m_vertices(vertices)
        , m_facets(facets)
    {}
    virtual ~GenuineMeshGeometry() = default;

public:
    virtual Index get_dim() const override { return safe_cast<Index>(m_vertices.cols()); }

    virtual Index get_num_vertices() const override { return safe_cast<Index>(m_vertices.rows()); }

    virtual Index get_num_facets() const override { return safe_cast<Index>(m_facets.rows()); }

    virtual Index get_vertex_per_facet() const override
    {
        return safe_cast<Index>(m_facets.cols());
    }

    virtual const VertexArray& get_vertices() const override { return m_vertices; }

    virtual const FacetArray& get_facets() const override { return m_facets; }

public:
    VertexArray& get_vertices() { return m_vertices; }

    FacetArray& get_facets() { return m_facets; }

protected:
    virtual VertexArray& get_vertices_ref() override { return m_vertices; }

    virtual FacetArray& get_facets_ref() override { return m_facets; }

private:
    VertexArray m_vertices;
    FacetArray m_facets;
};

template <typename _VertexArray, typename _FacetArray, typename Archive>
void serialize(std::shared_ptr<MeshGeometry<_VertexArray, _FacetArray>>& geometry, Archive& ar)
{
    if (ar.is_input()) {
        geometry = std::make_shared<GenuineMeshGeometry<_VertexArray, _FacetArray>>();
    }
    geometry->serialize_impl(ar);
}

} // namespace lagrange
