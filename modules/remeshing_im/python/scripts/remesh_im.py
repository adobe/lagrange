#!/usr/bin/env python

"""Remesh a triangle mesh into quad mesh using Lagrange Instant Meshes."""

import argparse
import lagrange


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quad", action="store_true", help="Generate quad mesh")
    parser.add_argument(
        "--num-facets",
        "-n",
        type=int,
        default=5000,
        help="Number of facets to generate",
    )
    parser.add_argument("input_mesh", help="The input mesh file")
    parser.add_argument("output_mesh", help="The output mesh file")
    return parser.parse_args()


def main():
    args = parse_args()
    mesh = lagrange.io.load_mesh(args.input_mesh, stitch_vertices=True)
    if mesh.is_hybrid:
        lagrange.triangulate_polygonal_facets(mesh)
    if args.quad:
        mesh = lagrange.remeshing_im.remesh(mesh, target_num_facets=args.num_facets)
        assert mesh.is_quad_mesh
    else:
        mesh = lagrange.remeshing_im.remesh(mesh, target_num_facets=args.num_facets, rosy=6, posy=3)
        assert mesh.is_triangle_mesh
    lagrange.io.save_mesh(args.output_mesh, mesh)


if __name__ == "__main__":
    main()
