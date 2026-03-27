#!/usr/bin/env python

#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "adobe-lagrange",
#     "polyscope",
#     "numpy",
# ]
# ///

#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

"""Visualize Helmholtz-Hodge decomposition of a smooth vector field.

Computes a smooth direction field on a triangle mesh using the polyddg module,
then decomposes it into exact (curl-free), co-exact (divergence-free), and
harmonic components. All four fields are displayed side-by-side with polyscope.

Usage:
    uv run modules/polyddg/python/examples/hodge_decomposition.py input_mesh.obj
"""

import argparse

import numpy as np
import polyscope as ps

import lagrange


def get_vertex_vectors(mesh, attr_name):
    """Extract a per-vertex 3D vector attribute as an (nv, 3) numpy array."""
    return np.array(mesh.attribute(attr_name).data).reshape(-1, 3)


def get_facet_indices(mesh):
    """Extract triangle indices as an (nf, 3) numpy array."""
    nf = mesh.num_facets
    indices = np.empty((nf, 3), dtype=np.uint32)
    for fi in range(nf):
        verts = mesh.get_facet_vertices(fi)
        indices[fi] = verts
    return indices


def get_vertex_tangent_bases(ops, nv):
    """Extract per-vertex tangent bases as (nv, 3) arrays for basisX and basisY."""
    basisX = np.empty((nv, 3))
    basisY = np.empty((nv, 3))
    for v in range(nv):
        B = np.array(ops.vertex_basis(v))  # 3x2
        basisX[v] = B[:, 0]
        basisY[v] = B[:, 1]
    return basisX, basisY


def project_to_tangent(field_3d, basisX, basisY):
    """Project 3D vectors to 2D tangent coordinates: (nv, 3) -> (nv, 2)."""
    u = np.sum(field_3d * basisX, axis=1)
    v = np.sum(field_3d * basisY, axis=1)
    return np.column_stack([u, v])


def register_mesh_with_field(name, vertices, faces, field_2d, basisX, basisY, offset, nrosy=1):
    """Register a polyscope mesh translated by offset, with a tangent vector field."""
    shifted = vertices + offset
    ps_mesh = ps.register_surface_mesh(name, shifted, faces, smooth_shade=True)
    ps_mesh.add_tangent_vector_quantity(
        "field", field_2d, basisX, basisY, n_sym=nrosy, enabled=True
    )
    return ps_mesh


def main():
    parser = argparse.ArgumentParser(
        description="Visualize Helmholtz-Hodge decomposition of a smooth vector field."
    )
    parser.add_argument("input_mesh", help="Path to input triangle mesh (e.g. .obj, .ply)")
    parser.add_argument(
        "--nrosy",
        type=int,
        default=1,
        help="N-rosy symmetry order for the direction field (default: 1)",
    )
    args = parser.parse_args()

    # Load mesh.
    mesh = lagrange.io.load_mesh(args.input_mesh)
    lagrange.triangulate_polygonal_facets(mesh)
    mesh.initialize_edges()
    print(f"Loaded mesh: {mesh.num_vertices} vertices, {mesh.num_facets} faces")

    # Compute smooth direction field.
    ops = lagrange.polyddg.DifferentialOperators(mesh)
    field_attr = "@smooth_direction_field"
    lagrange.polyddg.compute_smooth_direction_field(
        mesh,
        ops,
        nrosy=args.nrosy,
        direction_field_attribute=field_attr,
    )
    print("Computed smooth direction field")

    # Hodge decomposition.
    exact_attr = "@hodge_exact"
    coexact_attr = "@hodge_coexact"
    harmonic_attr = "@hodge_harmonic"
    lagrange.polyddg.hodge_decomposition_vector_field(
        mesh,
        ops,
        input_attribute=field_attr,
        exact_attribute=exact_attr,
        coexact_attribute=coexact_attr,
        harmonic_attribute=harmonic_attr,
        nrosy=args.nrosy,
    )
    print("Computed Hodge decomposition")

    # Extract data.
    vertices = np.array(mesh.vertices).reshape(-1, 3)
    faces = get_facet_indices(mesh)
    nv = mesh.num_vertices
    basisX, basisY = get_vertex_tangent_bases(ops, nv)

    V_input = get_vertex_vectors(mesh, field_attr)
    V_exact = get_vertex_vectors(mesh, exact_attr)
    V_coexact = get_vertex_vectors(mesh, coexact_attr)
    V_harmonic = get_vertex_vectors(mesh, harmonic_attr)

    # Print norms.
    print(f"  ||V_input||   = {np.linalg.norm(V_input):.6f}")
    print(f"  ||V_exact||   = {np.linalg.norm(V_exact):.6f}")
    print(f"  ||V_coexact|| = {np.linalg.norm(V_coexact):.6f}")
    print(f"  ||V_harmonic||= {np.linalg.norm(V_harmonic):.6f}")
    print(f"  ||residual||  = {np.linalg.norm(V_input - V_exact - V_coexact - V_harmonic):.2e}")

    # Project 3D vectors to 2D tangent coordinates for polyscope.
    T_input = project_to_tangent(V_input, basisX, basisY)
    T_exact = project_to_tangent(V_exact, basisX, basisY)
    T_coexact = project_to_tangent(V_coexact, basisX, basisY)
    T_harmonic = project_to_tangent(V_harmonic, basisX, basisY)

    # Compute bounding box for side-by-side layout.
    bbox_size = vertices.max(axis=0) - vertices.min(axis=0)
    spacing = bbox_size[0] * 1.3
    nrosy = args.nrosy

    # Visualize with polyscope.
    ps.init()
    ps.set_up_dir("z_up")

    register_mesh_with_field("Input", vertices, faces, T_input, basisX, basisY, [0, 0, 0], nrosy)
    register_mesh_with_field(
        "Exact (curl-free)", vertices, faces, T_exact, basisX, basisY, [spacing, 0, 0]
    )
    register_mesh_with_field(
        "Co-exact (div-free)",
        vertices,
        faces,
        T_coexact,
        basisX,
        basisY,
        [2 * spacing, 0, 0],
    )
    register_mesh_with_field(
        "Harmonic", vertices, faces, T_harmonic, basisX, basisY, [3 * spacing, 0, 0]
    )

    ps.show()


if __name__ == "__main__":
    main()
