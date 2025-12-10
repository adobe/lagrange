#!/usr/bin/env python

#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
"""Print basic information about a mesh file"""

import argparse
import colorama  # type: ignore
import json
import lagrange
import numpy as np
import pathlib
import platform

if platform.system() == "Windows":
    colorama.just_fix_windows_console()


def print_header(message):
    print(colorama.Fore.YELLOW + colorama.Style.BRIGHT + message + colorama.Style.RESET_ALL)


def print_green(message):
    print(colorama.Fore.GREEN + message + colorama.Style.RESET_ALL)


def print_red(message):
    print(colorama.Fore.RED + message + colorama.Style.RESET_ALL)


def print_section_header(val):
    print_green(f"{val:_^55}")


def print_property(name, val, expected=None):
    if expected is not None and val != expected:
        print_red(f"{name:-<48}: {val}")
    else:
        print(f"{name:-<48}: {val}")


def print_basic_info(mesh, info):
    print_section_header("Basic information")
    print(f"dim: {mesh.dimension}")

    # Vertex/facet/edge/corner count
    num_vertices = mesh.num_vertices
    num_facets = mesh.num_facets
    num_edges = mesh.num_edges
    num_corners = mesh.num_corners
    info["num_vertices"] = num_vertices
    info["num_facets"] = num_facets
    info["num_edges"] = num_edges
    info["num_corners"] = num_corners
    print(f"#v: {num_vertices:<10}#f: {num_facets:<10}#e: {num_edges:<10}#c: {num_corners:<10}")

    # Mesh bbox
    bbox_min = np.amin(mesh.vertices, axis=0)
    bbox_max = np.amax(mesh.vertices, axis=0)
    if mesh.dimension == 3:
        print(f"bbox min: [{bbox_min[0]:>10.3f} {bbox_min[1]:>10.3f} {bbox_min[2]:>10.3f}]")
        print(f"bbox max: [{bbox_max[0]:>10.3f} {bbox_max[1]:>10.3f} {bbox_max[2]:>10.3f}]")
    elif mesh.dimension == 2:
        print(f"bbox min: [{bbox_min[0]:>10.3f} {bbox_min[1]:>10.3f}]")
        print(f"bbox max: [{bbox_max[0]:>10.3f} {bbox_max[1]:>10.3f}]")
    else:
        print_red(f"Unsupported dimension: {mesh.dimension}")

    # vertex_per_facet
    if mesh.is_regular:
        if mesh.vertex_per_facet == 3:
            print("facet type: triangles")
        elif mesh.vertex_per_facet == 4:
            print("facet type: quads")
        else:
            print(f"facet type: polygons ({mesh.vertex_per_facet})")

        info["is_regular"] = True
        info["vertex_per_facet"] = mesh.vertex_per_facet
    else:
        print_red("facet type: hybrid")
        info["is_regular"] = False

        two_gons = 0
        triangles = 0
        quads = 0
        polygons = 0
        for fid in range(mesh.num_facets):
            f_size = mesh.get_facet_size(fid)
            match f_size:
                case 2:
                    two_gons += 1
                case 3:
                    triangles += 1
                case 4:
                    quads += 1
                case _:
                    polygons += 1
        if two_gons > 0:
            print_red(f"  # 2-gons: {two_gons}")
        if triangles > 0:
            print(f"  # triangles: {triangles}")
        if quads > 0:
            print(f"  # quads: {quads}")
        if polygons > 0:
            print(f"  # polygons (n>4): {polygons}")


def print_extra_info(mesh, info):
    # components
    num_components = lagrange.compute_components(mesh)
    print_property("num components", num_components)
    info["num_components"] = num_components

    # boundary check
    bd_edges = lagrange.extract_boundary_edges(mesh)
    print_property("closed", len(bd_edges) == 0, True)
    if len(bd_edges) > 0:
        print_property("num boundary edges", len(bd_edges), 0)
        bd_loops = lagrange.extract_boundary_loops(mesh)
        print_property("num boundary loops", len(bd_loops), 0)

    # manifold check
    edge_manifold = lagrange.is_edge_manifold(mesh)
    vertex_manifold = lagrange.is_vertex_manifold(mesh)

    info["edge_manifold"] = edge_manifold
    info["vertex_manifold"] = vertex_manifold
    info["manifold"] = edge_manifold and vertex_manifold
    print_property("manifold", info["manifold"], True)

    if not vertex_manifold:
        vertex_manifold_attr_id = lagrange.compute_vertex_is_manifold(mesh)
        vertex_manifold_data = mesh.attribute(vertex_manifold_attr_id).data
        num_non_manifold_vertices = int(np.sum(vertex_manifold_data == 0))
        print_property("non-manifold vertices", num_non_manifold_vertices, 0)
        info["nonmanifold_vertices"] = num_non_manifold_vertices
    else:
        print_property("vertex manifold", vertex_manifold, True)
        info["nonmanifold_vertices"] = 0

    if not edge_manifold:
        assert not vertex_manifold  # not edge manifold implies not vertex manifold
        edge_manifold_attr_id = lagrange.compute_edge_is_manifold(mesh)
        edge_manifold_data = mesh.attribute(edge_manifold_attr_id).data
        num_non_manifold_edges = int(np.sum(edge_manifold_data == 0))
        print_property("non-manifold edges", num_non_manifold_edges, 0)
        info["nonmanifold_edges"] = num_non_manifold_edges
    else:
        print_property("edge manifold", edge_manifold, True)
        info["nonmanifold_edges"] = 0

    # orientability check
    is_orientable = lagrange.is_oriented(mesh)
    info["orientable"] = is_orientable
    if not is_orientable:
        edge_oriented_attr_id = lagrange.compute_edge_is_oriented(mesh)
        edge_oriented_data = mesh.attribute(edge_oriented_attr_id).data
        num_non_oriented_edges = int(np.sum(edge_oriented_data == 0))
        print_property("non-oriented edges", num_non_oriented_edges, 0)
        info["nonoriented_edges"] = num_non_oriented_edges
    else:
        print_property("orientable", is_orientable, True)
        info["nonoriented_edges"] = 0

    # Degeneracy check
    num_degenerate_facets = lagrange.detect_degenerate_facets(mesh)
    info["degenerate_facets"] = num_degenerate_facets
    print_property("num degenerate facets", len(num_degenerate_facets), 0)

    # UV check
    if (
        mesh.is_triangle_mesh
        and mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV) != []
    ):
        uv_attr_id = mesh.get_matching_attribute_id(usage=lagrange.AttributeUsage.UV)
        if not mesh.is_attribute_indexed(uv_attr_id):
            uv_attr_id = lagrange.map_attribute(
                mesh, uv_attr_id, "_indexed_uv", lagrange.AttributeElement.Indexed
            )
        distortion_id = lagrange.compute_uv_distortion(
            mesh, mesh.get_attribute_name(uv_attr_id), metric=lagrange.DistortionMetric.AreaRatio
        )
        distortion = mesh.attribute(distortion_id).data
        num_flipped_uv = int(np.sum(distortion < 0))
        print_property("num flipped UV facets", num_flipped_uv, 0)
        info["num_flipped_uv"] = num_flipped_uv


def usage_to_str(usage):
    return str(usage).split(".")[-1]


def element_to_str(element):
    return str(element).split(".")[-1]


def dtype_to_str(dtype: np.dtype):
    match dtype:
        case np.float32:
            dtype_str = "float32"
        case np.float64:
            dtype_str = "float64"
        case np.int32:
            dtype_str = "int32"
        case np.int64:
            dtype_str = "int64"
        case np.uint32:
            dtype_str = "uint32"
        case np.uint64:
            dtype_str = "uint64"
        case _:
            dtype_str = str(dtype)
    return dtype_str


def print_attributes(mesh):
    for id in mesh.get_matching_attribute_ids():
        name = mesh.get_attribute_name(id)
        if name.startswith("@"):
            continue
        is_indexed = mesh.is_attribute_indexed(id)
        if is_indexed:
            attr = mesh.indexed_attribute(id)
        else:
            attr = mesh.attribute(id)
        usage = usage_to_str(attr.usage)
        element_type = element_to_str(attr.element_type)
        num_channels = attr.num_channels
        if not is_indexed:
            dtype = dtype_to_str(attr.dtype)
        else:
            dtype = dtype_to_str(attr.values.dtype)

        print(f"Attribute {colorama.Fore.GREEN}{name}{colorama.Style.RESET_ALL} ({dtype})")
        print(f"  id:{id:<5}usage: {usage:<10}elem: {element_type:<10}channels: {num_channels}")


def load_info(mesh_file):
    mesh_file = pathlib.Path(mesh_file)
    info_file = mesh_file.with_suffix(".json")
    info = {}
    if info_file.exists():
        with open(info_file, "r") as fin:
            try:
                info = json.load(fin)
            except ValueError:
                print_red(f"Cannot parse {info_file}, overwriting it")
    return info


def save_info(mesh_file, info):
    mesh_file = pathlib.Path(mesh_file)
    info_file = mesh_file.with_suffix(".json")

    with open(info_file, "w") as fout:
        json.dump(info, fout, indent=4, sort_keys=True)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--extended",
        "-x",
        action="store_true",
        help="check for extended information such as number of components, manifoldness and more",
    )
    parser.add_argument(
        "--attribute",
        "-a",
        action="store_true",
        help="print attribute information",
    )
    parser.add_argument(
        "--export", "-e", action="store_true", help="export stats into a .info file"
    )
    parser.add_argument("input_mesh", help="input mesh file")
    return parser.parse_args()


def main():
    args = parse_args()
    mesh = lagrange.io.load_mesh(args.input_mesh, quiet=True)
    mesh.initialize_edges()
    info = load_info(args.input_mesh)

    header = f"Summary of {args.input_mesh}"
    print_header(f"{header:=^55}")
    print_basic_info(mesh, info)

    if args.extended:
        print_extra_info(mesh, info)
    if args.attribute:
        print_attributes(mesh)

    if args.export:
        save_info(args.input_mesh, info)


if __name__ == "__main__":
    main()
