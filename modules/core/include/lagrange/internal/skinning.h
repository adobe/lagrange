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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/attribute_names.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <numeric>
#include <vector>

/**
 * This file contains commonly used functions related to skinning deformation on a mesh.
 *
 * `skinning_deform` deforms a mesh with weights.
 *
 * `skinning_extract_n` takes a weight matrix |V| x |H| and outputs indexed weights, up to n per
 * vertex.
 *
 * `weights_to_mesh_attribute` and `weights_to_indexed_mesh_attribute` save a weights matrix as
 * attributes.
 */

namespace lagrange::internal {

/**
 * Performs linear blend skinning deformation on a mesh..
 * 
 * @param[in,out] mesh              vertices of this mesh will be modified
 * @param[in] original_vertices     original positions of vertices
 * @param[in] transforms            vector of eigen affine transforms, describe the global movement of each handle/joint
 * @param[in] weights               |V| x |handle| weight matrix
 * @param[in] weight_complement     optional, acts as weights for an extra handle that does not move.
 * 
 */
template <typename Scalar, typename Index>
void skinning_deform(
    SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::Attribute<Scalar>& original_vertices,
    const std::vector<Eigen::Transform<Scalar, 3, Eigen::TransformTraits::Affine>>& transforms,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& weights,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& weight_complement = {})
{
    la_runtime_assert((size_t)weights.cols() == transforms.size());
    la_runtime_assert(weights.rows() == mesh.get_num_vertices());
    la_runtime_assert(
        (weight_complement.rows() == 0) || (weight_complement.rows() == mesh.get_num_vertices()));

    const Index num_vertices = mesh.get_num_vertices();
    const size_t num_handles = transforms.size();

    auto original_view = lagrange::matrix_view(original_vertices);
    auto verts = lagrange::vertex_ref(mesh);
    verts.setZero();

    for (Index v = 0; v < num_vertices; ++v) {
        const Eigen::Matrix<Scalar, 3, 1> orig = original_view.row(v);
        Scalar weight_sum = 0;
        for (size_t h = 0; h < num_handles; ++h) {
            Scalar weight = weights(v, h);
            if (weight > 0) {
                verts.row(v) += weight * (transforms[h] * orig);
            }
            weight_sum += weight;
        }
        if (weight_complement.rows() > 0 && weight_complement(v) > 0) {
            verts.row(v) += (weight_complement(v) * orig);
            weight_sum += weight_complement(v);
        }
        if (weight_sum > 0) {
            verts.row(v) /= weight_sum;
        }
    }
}

/**
 * Performs linear blend skinning on a mesh, using weights information from the mesh attributes..
 * 
 * @param[in,out] mesh              vertices of this mesh will be modified
 * @param[in] original_vertices     original positions of vertices
 * @param[in] transforms            vector of eigen affine transforms, describe the global movement of each handle/joint
 * 
 */
template <typename Scalar, typename Index>
void skinning_deform(
    SurfaceMesh<Scalar, Index>& mesh,
    const Attribute<Scalar>& original_vertices,
    const std::vector<Eigen::Transform<Scalar, 3, Eigen::TransformTraits::Affine>>& transforms)
{
    auto weight_id = mesh.get_attribute_id(AttributeName::weight);
    la_runtime_assert(weight_id != invalid_attribute_id());

    auto& weight_attr = mesh.template get_attribute<Scalar>(weight_id);

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> weights = matrix_view(weight_attr);
    skinning_deform(mesh, original_vertices, transforms, weights);
}


template <typename Scalar, typename Index>
struct SkinningExtractNResult
{
    /**
     * |V| x n weights (scalar). For each vertex, this will include the n most important
     * weights.
     */
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> weights;

    /**
     * |V| x n indices (index). For each vertex, this will be the index of the n most
     * important.
     */
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic> indices;
};

/**
 * From a weight matrix |V| x |H|, constructs a weight matrix |V| x n,
 * where n is an arbitrary contraint (typically 4 or 8).
 * 
 * @param[in] weights               |V| x |handle| weight matrix
 * @param[in] n                     max number of weights for each vertex
 * @param[in] weight_complement     optional, acts as weights for an extra handle that does not move.
 * 
 * @return                          |V| x n weights and |V| x n indices (see struct above). 
 */
template <typename Scalar, typename Index>
SkinningExtractNResult<Scalar, Index> skinning_extract_n(
    const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& weights,
    int n,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& weight_complement =
        Eigen::Matrix<Scalar, Eigen::Dynamic, 1>())
{
    const Index num_vertices = Index(weights.rows());
    const Index num_handles = Index(weights.cols());
    Index num_implicit_handle = 0;

    la_runtime_assert(num_handles > 0);
    if (weight_complement.rows() > 0 &&
        weight_complement.maxCoeff() > std::numeric_limits<Scalar>::epsilon()) {
        la_runtime_assert(weight_complement.rows() == num_vertices);
        // the weights do not sum up to 1, so we assume there is an implicit fixed handle
        num_implicit_handle = 1;
    }

    // Max number of handles
    const Index num_handles_max = Index(n) - num_implicit_handle;
    const Index num_handles_used = std::min(num_handles, num_handles_max);


    SkinningExtractNResult<Scalar, Index> result;
    result.weights.resize(num_vertices, n);
    result.indices.resize(num_vertices, n);
    if (num_handles < Index(n)) {
        // only need to initialize in this case
        result.weights.setZero();
        result.indices.setZero();
    }

    if (num_handles <= num_handles_max) {
        // we have more handle slots than handles, so just copy them over
        la_runtime_assert(num_handles == num_handles_used);
        for (Index i = 0; i < num_vertices; ++i) {
            for (Index j = 0; j < num_handles; ++j) {
                result.indices(i, j) = j;
                result.weights(i, j) = weights(i, j);
            }
        }

    } else {
        // we only pick some of the handles, find and copy the n most important ones

        for (Index i = 0; i < num_vertices; ++i) {
            std::vector<Index> tmp(num_handles);
            std::iota(tmp.begin(), tmp.end(), 0);
            std::partial_sort(
                tmp.begin(),
                tmp.begin() + num_handles_used,
                tmp.end(),
                [&weights, i](const Index a, const Index b) -> bool {
                    return weights(i, a) > weights(i, b);
                });

            for (Index j = 0; j < num_handles_used; ++j) {
                result.indices(i, j) = tmp[j];
                result.weights(i, j) = weights(i, tmp[j]);
            }
        }
    }

    if (num_implicit_handle > 0) {
        // add the implicit handle
        for (Index i = 0; i < num_vertices; ++i) {
            result.indices(i, num_handles_used) = num_handles_used;
            result.weights(i, num_handles_used) = weight_complement(i);
        }
    }

    // normalize if we changed the active handles
    if (num_handles > num_handles_max) {
        for (Index i = 0; i < num_vertices; ++i) {
            Scalar s = result.weights.row(i).sum();
            la_runtime_assert(s > 0);
            result.weights.row(i) /= s;
        }
    }

    return result;
}

/**
 * Imports the weights matrix as weight attributes of the mesh.
 * 
 * @param[in,out] mesh      Mesh to modify
 * @param[in] weights       weights matrix
 * 
 * @return                  new weights attribute id
 */
template <typename Scalar, typename Index, typename WeightScalar = Scalar>
lagrange::AttributeId weights_to_mesh_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Matrix<WeightScalar, Eigen::Dynamic, Eigen::Dynamic>& weights)
{
    return mesh.template create_attribute<WeightScalar>(
        AttributeName::weight,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        weights.cols(),
        {weights.data(), size_t(weights.size())});
}

/**
 * Imports the weights matrix as indexed weight attributes of the mesh.
 * 
 * @param[in,out] mesh  Mesh to modify
 * @param[in] weights   Weights matrix
 * @param[in] n         max weights per vertex
 * 
 * @return              pair of new attribute ids: index and weight
 */
template <
    typename Scalar, 
    typename Index,
    typename WeightScalar = Scalar,
    typename WeightIndex = Index>
std::pair<lagrange::AttributeId, lagrange::AttributeId> weights_to_indexed_mesh_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Matrix<WeightScalar, Eigen::Dynamic, Eigen::Dynamic>& weights,
    int n)
{
    auto result = skinning_extract_n<WeightScalar, WeightIndex>(weights, n);
    auto bone_id = mesh.template create_attribute<WeightIndex>(
        AttributeName::indexed_joint,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        result.indices.cols(),
        {result.indices.data(), size_t(result.indices.size())});

    auto weight_id = mesh.template create_attribute<WeightScalar>(
        AttributeName::indexed_weight,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        result.weights.cols(),
        {result.weights.data(), size_t(result.weights.size())});
    return {bone_id, weight_id};
}


} // namespace lagrange::internal
