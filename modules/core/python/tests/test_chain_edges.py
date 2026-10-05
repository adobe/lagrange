#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import lagrange
import numpy as np
import pytest


class TestChainEdges:
    def test_chain_directed_edges(self):
        edges = np.array([[0, 1], [1, 2], [2, 0], [3, 4]], dtype=np.uint32)

        loops, chains = lagrange.chain_edges(edges, directed=True)

        assert sorted(loops[0]) == [0, 1, 2]
        assert chains == [[3, 4]]

    def test_chain_edges_orientation(self):
        edges = np.array([[0, 1], [2, 1], [2, 0]], dtype=np.uint32)

        loops, _ = lagrange.chain_edges(edges, directed=False)
        assert len(loops) == 1
        assert sorted(loops[0]) == [0, 1, 2]

        loops, chains = lagrange.chain_edges(edges, directed=True)
        assert loops == []
        assert sum(len(chain) - 1 for chain in chains) == 3

    def test_chain_undirected_edges_with_kwargs(self):
        edges = np.array([[10, 11], [11, 12], [12, 10]], dtype=np.uint32)

        loops, chains = lagrange.chain_edges(edges, directed=False, output_edge_index=True)

        assert len(loops) == 1
        assert sorted(loops[0]) == [0, 1, 2]
        assert chains == []

    def test_chain_edges_close_loop(self):
        edges = np.array([[0, 1], [1, 2], [2, 0]], dtype=np.uint32)

        loops, chains = lagrange.chain_edges(
            edges, directed=True, close_loop_with_identical_vertices=True
        )

        assert loops == [[0, 1, 2, 0]]
        assert chains == []

    @pytest.mark.parametrize("directed", [True, False])
    def test_chain_edges_empty(self, directed):
        loops, chains = lagrange.chain_edges(np.empty((0, 2), dtype=np.uint32), directed=directed)

        assert loops == []
        assert chains == []

    def test_chain_edges_keyword_only_options(self):
        edges = np.array([[0, 1]], dtype=np.uint32)

        with pytest.raises(TypeError):
            lagrange.chain_edges(edges)  # ty: ignore[missing-argument]
        with pytest.raises(TypeError):
            lagrange.chain_edges(edges, True)  # ty: ignore[missing-argument, too-many-positional-arguments]

    @pytest.mark.parametrize(
        "edges",
        [
            np.array([0, 1], dtype=np.uint32),
            np.array([[lagrange.invalid_index, 0]], dtype=np.uint32),
        ],
    )
    def test_chain_edges_reject_invalid_input(self, edges):
        with pytest.raises(RuntimeError):
            lagrange.chain_edges(edges, directed=True)
