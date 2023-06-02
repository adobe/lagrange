/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange {

///
/// @ingroup    group-utils
///
/// @{
///
/// A simple loop is defined as a set of connected edges whose starting and ending vertex is the
/// same. All vertices involved in a simple loop has exact two incident edges. For example, the
/// letter `O` contains one simple loop.
///
/// An ear loop is defined as a set of connected edges whose starting and ending vertex is the same.
/// All vertices except the start/end vertex has exact two incident edges. For example, the digit
/// `8` contains two ear loops.
///
/// A chain is defined as a set of connected edges whose starting and ending vertex is different.
/// All vertices in a chain except the end points have exactly two incident edges.
/// For example, the digit `2` contains one chain.
///
/// A chain is called a simple chain its both of its end points are not connected to any other
/// edges. I.e. they have degree 1. For example, the `=` sign has two simple chains.
///
/// A chain is called a hanging chain if one of its end vertices is not connected to any other
/// edges. For example, the digit `6` contains 1 hanging chain and a ear loop.
///

///
/// Options for chain_directed_edges and chain_undirected_edges.
///
struct ChainEdgesOptions
{
    /// If true, the output will be a list of edges, otherwise a list of vertices.
    bool output_edge_index = false;

    /// If true, the first and last vertices of a loop will be identical.
    bool close_loop_with_identical_vertices = false;
};

///
/// Result struct holding the loops and chains extracted from a set of edges.
///
/// @tparam Index The Index type.
///
template <typename Index>
struct ChainEdgesResult
{
    std::vector<std::vector<Index>> loops;
    std::vector<std::vector<Index>> chains;
};

///
/// Chain a set of directed edges into loops and chains.
///
/// This methods guarantees that all simple loops and simple chains are extracted. This method will
/// also iteratively extract all ear loops and hanging chains.
///
/// @tparam Index     The Index type.
///
/// @param edges      The input set of edges.
/// @param options    Options controlling the behavior of this function.
///
/// @return           A set of loops and chains extracted from the input edge set.
///
/// Both loops and chains are stored as an array of edges indices if `options.output_edge_index` is
/// true, otherwise, they are an array of vertex indices.
///
/// If `options.output_edge_vertex` is false and `options.close_loop_with_identical_vertices` is
/// true, the first and last vertices of a loop is identical.
///
/// @see chain_undirected_edges
///
template <typename Index>
ChainEdgesResult<Index> chain_directed_edges(
    const span<const Index> edges,
    const ChainEdgesOptions& options = {});

///
/// Chain a set of undirected edges into loops and chains.
///
/// This methods guarantees that all simple loops and simple chains are extracted. This method will
/// also iteratively extract all ear loops and hanging chains.
///
/// @tparam Index     The Index type.
///
/// @param edges      The input set of edges.
/// @param options    Options controlling the behavior of this function.
///
/// @return           A set of loops and chains extracted from the input edge set.
///
/// Both loops and chains are stored as an array of edges indices if `options.output_edge_index` is
/// true, otherwise, they are an array of vertex indices.
///
/// If `options.output_edge_vertex` is false and `options.close_loop_with_identical_vertices` is
/// true, the first and last vertices of a loop is identical.
///
/// @see chain_directed_edges
///
template <typename Index>
ChainEdgesResult<Index> chain_undirected_edges(
    const span<const Index> edges,
    const ChainEdgesOptions& options = {});

/// @}

} // namespace lagrange
