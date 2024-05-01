#!/usr/bin/env python
"""Print basic information about a mesh file"""

import argparse
import lagrange
import colorama
import json
import pathlib
import numpy as np

colorama.just_fix_windows_console()


def print_header(message):
    print(
        colorama.Fore.YELLOW
        + colorama.Style.BRIGHT
        + message
        + colorama.Style.RESET_ALL
    )


def print_green(message):
    print(colorama.Fore.GREEN + message + colorama.Style.RESET_ALL)


def print_red(message):
    print(colorama.Fore.RED + message + colorama.Style.RESET_ALL)


def print_section_header(val):
    print_green("{:_^55}".format(val))


def print_property(name, val, expected=None):
    if expected is not None and val != expected:
        print_red("{:-<48}: {}".format(name, val))
    else:
        print("{:-<48}: {}".format(name, val))


def print_basic_info(mesh, info):
    print_section_header("Basic information")
    print("dim: {}".format(mesh.dimension))

    # Vertex/facet/edge/corner count
    num_vertices = mesh.num_vertices
    num_facets = mesh.num_facets
    num_edges = mesh.num_edges
    num_corners = mesh.num_corners
    info["num_vertices"] = num_vertices
    info["num_facets"] = num_facets
    info["num_edges"] = num_edges
    info["num_corners"] = num_corners
    print(
        f"#v: {num_vertices:<10}#f: {num_facets:<10}#e: {num_edges:<10}#c: {num_corners:<10}"
    )

    # Mesh bbox
    bbox_min = np.amin(mesh.vertices, axis=0)
    bbox_max = np.amax(mesh.vertices, axis=0)
    print(f"bbox min: [{bbox_min[0]:>10.3f} {bbox_min[1]:>10.3f} {bbox_min[2]:>10.3f}]")
    print(f"bbox max: [{bbox_max[0]:>10.3f} {bbox_max[1]:>10.3f} {bbox_max[2]:>10.3f}]")

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
        print_red(f"facet type: hybrid")
        info["is_regular"] = True


def print_extra_info(mesh, info):
    # components
    num_components = lagrange.compute_components(mesh)
    print_property("num_components", num_components)
    info["num_components"] = num_components

    # manifold check
    edge_manifold = lagrange.is_edge_manifold(mesh)
    if edge_manifold:
        vertex_manifold = lagrange.is_vertex_manifold(mesh)
    else:
        vertex_manifold = False
    info["edge_manifold"] = edge_manifold
    info["vertex_manifold"] = vertex_manifold
    info["manifold"] = edge_manifold and vertex_manifold
    print_property("manifold", info["manifold"], True)
    print_property("vertex manifold", vertex_manifold, True)
    print_property("edge manifold", edge_manifold, True)

    # Degeneracy check
    num_degenerate_facets = lagrange.detect_degenerate_facets(mesh)
    info["degenerate_facets"] = num_degenerate_facets
    print_property("num degenerate facets", len(num_degenerate_facets), 0)


def usage_to_str(usage):
    return str(usage).split(".")[-1]


def element_to_str(element):
    return str(element).split(".")[-1]


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

        print(f"Attribute {colorama.Fore.GREEN}{name}{colorama.Style.RESET_ALL}")
        print(
            f"  id:{id:<5}usage: {usage:<10}elem: {element_type:<10}channels: {num_channels}"
        )


def load_info(mesh_file):
    mesh_file = pathlib.Path(mesh_file)
    info_file = mesh_file.with_suffix(".json")
    info = {}
    if info_file.exists():
        with open(info_file, "r") as fin:
            try:
                info = json.load(fin)
            except ValueError:
                print_red("Cannot parse {}, overwriting it".format(info_file))
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
    mesh = lagrange.io.load_mesh(args.input_mesh)
    mesh.initialize_edges()
    info = load_info(args.input_mesh)

    header = "Summary of {}".format(args.input_mesh)
    print_header("{:=^55}".format(header))
    print_basic_info(mesh, info)

    if args.extended:
        print_extra_info(mesh, info)
    if args.attribute:
        print_attributes(mesh)

    if args.export:
        save_info(args.input_mesh, info)


if __name__ == "__main__":
    main()
