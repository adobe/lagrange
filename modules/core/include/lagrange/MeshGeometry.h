/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <lagrange/common.h>
#include <lagrange/utils/warning.h>

namespace lagrange {

template <typename _VertexArray, typename _FacetArray>
class MeshGeometry
{
public:
    using VertexArray = _VertexArray;
    using FacetArray = _FacetArray;
    using VertexType =
        Eigen::Matrix<typename VertexArray::Scalar, 1, VertexArray::ColsAtCompileTime>;


    using Scalar = typename VertexArray::Scalar;
    using Index = typename FacetArray::Scalar;

public:
    MeshGeometry() = default;
    MeshGeometry(const VertexArray& /*vertices*/, const FacetArray& /*facets*/) {}
    virtual ~MeshGeometry() = default;

    virtual Index get_dim() const = 0;
    virtual Index get_num_vertices() const = 0;
    virtual Index get_num_facets() const = 0;
    virtual Index get_vertex_per_facet() const = 0;

    virtual const VertexArray& get_vertices() const = 0;
    virtual const FacetArray& get_facets() const = 0;

    virtual VertexArray& get_vertices_ref() = 0;
    virtual FacetArray& get_facets_ref() = 0;

public:
    template <typename Derived>
    void import_vertices(Eigen::MatrixBase<Derived>& vertices)
    {
        auto& member_vertices = this->get_vertices_ref();
        move_data(vertices, member_vertices);
    }

    template <typename Derived>
    void import_facets(Eigen::MatrixBase<Derived>& facets)
    {
        auto& member_facets = this->get_facets_ref();
        move_data(facets, member_facets);
    }

    template <typename Derived>
    void export_vertices(Eigen::MatrixBase<Derived>& vertices)
    {
        auto& member_vertices = this->get_vertices_ref();
        move_data(member_vertices, vertices);
    }

    template <typename Derived>
    void export_facets(Eigen::MatrixBase<Derived>& facets)
    {
        auto& member_facets = this->get_facets_ref();
        move_data(member_facets, facets);
    }

    template <typename Archive>
    void serialize_impl(Archive& ar)
    {
        LA_IGNORE_SHADOW_WARNING_BEGIN
        enum { VERTICES = 0, FACETS = 1 };
        ar.object([&](auto& ar) {
            ar("vertices", VERTICES) & get_vertices_ref();
            ar("facets", FACETS) & get_facets_ref();
        });
        LA_IGNORE_SHADOW_WARNING_END
    }
};

template <typename _VertexArray, typename _FacetArray, typename Archive>
void serialize(MeshGeometry<_VertexArray, _FacetArray>& geometry, Archive& ar)
{
    geometry.serialize_impl(ar);
}

} // namespace lagrange
