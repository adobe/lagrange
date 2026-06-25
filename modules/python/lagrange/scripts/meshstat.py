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
"""Print basic information about a mesh file."""

from __future__ import annotations

import argparse
import json
import logging
import pathlib
import platform
from contextlib import ExitStack, contextmanager

import colorama
import lagrange
import numpy as np

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Display helpers (colorized output for human-readable summary)
# ---------------------------------------------------------------------------


def _colored(text: str, color: object) -> str:
    return f"{color}{text}{colorama.Style.RESET_ALL}"


def print_header(message: str) -> None:
    print(_colored(message, colorama.Fore.YELLOW + colorama.Style.BRIGHT))


def print_section(message: str) -> None:
    print(_colored(f"{message:_^55}", colorama.Fore.GREEN))


def print_property(name: str, value, expected=None) -> None:
    line = f"{name:-<48}: {value}"
    if expected is not None and value != expected:
        line = _colored(line, colorama.Fore.RED)
    print(line)


def print_bad(message: str) -> None:
    print(_colored(message, colorama.Fore.RED))


# ---------------------------------------------------------------------------
# Stats and JSON helpers
# ---------------------------------------------------------------------------


def compute_stats(values: np.ndarray, percentiles=(1, 10, 25, 75, 90, 99)) -> dict:
    """Return summary statistics for a 1-D numpy array, ignoring non-finite values."""
    arr = np.asarray(values).ravel()
    finite = arr[np.isfinite(arr)] if arr.size else arr
    if finite.size == 0:
        return {
            "count": int(arr.size),
            "num_invalid": int(arr.size),
            "min": None,
            "max": None,
            "mean": None,
            "median": None,
            "std": None,
            "percentiles": {},
        }
    return {
        "count": int(arr.size),
        "num_invalid": int(arr.size - finite.size),
        "min": float(finite.min()),
        "max": float(finite.max()),
        "mean": float(finite.mean()),
        "median": float(np.median(finite)),
        "std": float(finite.std()),
        "percentiles": {str(p): float(np.percentile(finite, p)) for p in percentiles},
    }


def to_jsonable(value):
    """Convert numpy scalars/arrays/Path objects to JSON-serializable types."""
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, pathlib.PurePath):
        return str(value)
    raise TypeError(f"Object of type {type(value).__name__} is not JSON serializable")


def save_info(mesh_file: str, info: dict) -> pathlib.Path:
    info_file = pathlib.Path(mesh_file).with_suffix(".json")
    with open(info_file, "w") as fout:
        json.dump(info, fout, indent=4, sort_keys=True, default=to_jsonable)
    return info_file


# ---------------------------------------------------------------------------
# Basic and extended sections
# ---------------------------------------------------------------------------


def _facet_type(mesh) -> tuple[str, dict | None]:
    if mesh.is_regular:
        if mesh.vertex_per_facet == 3:
            return "triangles", None
        if mesh.vertex_per_facet == 4:
            return "quads", None
        if mesh.vertex_per_facet == 2:
            return "two_gons", None
        return f"polygons ({mesh.vertex_per_facet})", None

    counts = {"two_gons": 0, "triangles": 0, "quads": 0, "polygons": 0}
    for fid in range(mesh.num_facets):
        f_size = mesh.get_facet_size(fid)
        if f_size == 2:
            counts["two_gons"] += 1
        elif f_size == 3:
            counts["triangles"] += 1
        elif f_size == 4:
            counts["quads"] += 1
        else:
            counts["polygons"] += 1
    return "hybrid", counts


def collect_basic_info(mesh, info: dict) -> None:
    """Populate ``info["basic"]`` with mesh shape/bbox metadata.

    Initializes mesh edges if needed so ``num_edges`` is reported correctly
    (``SurfaceMesh.num_edges`` returns 0 until ``initialize_edges`` has been
    called).
    """
    if not mesh.has_edges:
        mesh.initialize_edges()
    facet_type, facet_counts = _facet_type(mesh)

    if facet_counts is None:
        n = int(mesh.vertex_per_facet)
        num_f = int(mesh.num_facets)
        facet_counts = {
            "two_gons": num_f if n == 2 else 0,
            "triangles": num_f if n == 3 else 0,
            "quads": num_f if n == 4 else 0,
            "polygons": num_f if n not in (2, 3, 4) else 0,
        }

    basic: dict = {
        "dim": int(mesh.dimension),
        "num_vertices": int(mesh.num_vertices),
        "num_facets": int(mesh.num_facets),
        "num_edges": int(mesh.num_edges),
        "num_corners": int(mesh.num_corners),
        "facet_type": facet_type,
        "facet_counts": facet_counts,
    }
    if mesh.num_vertices > 0:
        bbox_min = np.amin(mesh.vertices, axis=0)
        bbox_max = np.amax(mesh.vertices, axis=0)
        bbox_extent = bbox_max - bbox_min
        basic["bbox_min"] = [float(x) for x in bbox_min]
        basic["bbox_max"] = [float(x) for x in bbox_max]
        basic["bbox_extent"] = [float(x) for x in bbox_extent]
        basic["bbox_diagonal"] = float(np.linalg.norm(bbox_extent))
        basic["max_extent"] = float(np.max(bbox_extent)) if bbox_extent.size else 0.0
    else:
        basic["bbox_min"] = None
        basic["bbox_max"] = None
        basic["bbox_extent"] = None
        basic["bbox_diagonal"] = 0.0
        basic["max_extent"] = 0.0
    info["basic"] = basic


def print_basic_info(mesh, info: dict) -> None:
    collect_basic_info(mesh, info)
    basic = info["basic"]
    print_section("Basic information")
    print(f"dim: {basic['dim']}")
    print(
        f"#v: {basic['num_vertices']:<10}#f: {basic['num_facets']:<10}"
        f"#e: {basic['num_edges']:<10}#c: {basic['num_corners']:<10}"
    )
    if basic["bbox_min"] is None:
        print("bbox: (empty mesh)")
    elif basic["dim"] == 3:
        bmin = basic["bbox_min"]
        bmax = basic["bbox_max"]
        print(f"bbox min: [{bmin[0]:>10.3f} {bmin[1]:>10.3f} {bmin[2]:>10.3f}]")
        print(f"bbox max: [{bmax[0]:>10.3f} {bmax[1]:>10.3f} {bmax[2]:>10.3f}]")
    elif basic["dim"] == 2:
        bmin = basic["bbox_min"]
        bmax = basic["bbox_max"]
        print(f"bbox min: [{bmin[0]:>10.3f} {bmin[1]:>10.3f}]")
        print(f"bbox max: [{bmax[0]:>10.3f} {bmax[1]:>10.3f}]")
    else:
        print_bad(f"Unsupported dimension: {basic['dim']}")
    if basic["facet_type"] == "hybrid":
        print_bad("facet type: hybrid")
        counts = basic["facet_counts"]
        if counts.get("two_gons", 0) > 0:
            print_bad(f"  # 2-gons: {counts['two_gons']}")
        if counts.get("triangles", 0) > 0:
            print(f"  # triangles: {counts['triangles']}")
        if counts.get("quads", 0) > 0:
            print(f"  # quads: {counts['quads']}")
        if counts.get("polygons", 0) > 0:
            print(f"  # polygons (n>4): {counts['polygons']}")
    else:
        print(f"facet type: {basic['facet_type']}")


def collect_extended_info(mesh, info: dict) -> None:
    """Populate ``info["extended"]`` with topology/manifoldness checks."""
    if not mesh.has_edges:
        mesh.initialize_edges()
    extended: dict = {}

    extended["num_components"] = int(lagrange.compute_components(mesh))

    bd_edges = lagrange.extract_boundary_edges(mesh)
    extended["num_boundary_edges"] = int(len(bd_edges))
    extended["closed"] = extended["num_boundary_edges"] == 0
    if not extended["closed"]:
        extended["num_boundary_loops"] = int(len(lagrange.extract_boundary_loops(mesh)))
    else:
        extended["num_boundary_loops"] = 0

    edge_manifold = bool(lagrange.is_edge_manifold(mesh))
    vertex_manifold = bool(lagrange.is_vertex_manifold(mesh))
    extended["edge_manifold"] = edge_manifold
    extended["vertex_manifold"] = vertex_manifold
    extended["manifold"] = edge_manifold and vertex_manifold

    if not vertex_manifold:
        attr_id = lagrange.compute_vertex_is_manifold(mesh)
        extended["nonmanifold_vertices"] = int(np.sum(mesh.attribute(attr_id).data == 0))
    else:
        extended["nonmanifold_vertices"] = 0

    if not edge_manifold:
        attr_id = lagrange.compute_edge_is_manifold(mesh)
        extended["nonmanifold_edges"] = int(np.sum(mesh.attribute(attr_id).data == 0))
    else:
        extended["nonmanifold_edges"] = 0

    is_orientable = bool(lagrange.is_oriented(mesh))
    extended["orientable"] = is_orientable
    if not is_orientable:
        attr_id = lagrange.compute_edge_is_oriented(mesh)
        extended["nonoriented_edges"] = int(np.sum(mesh.attribute(attr_id).data == 0))
    else:
        extended["nonoriented_edges"] = 0

    extended["num_degenerate_facets"] = int(len(lagrange.detect_degenerate_facets(mesh)))

    extended["euler_characteristic"] = (
        int(mesh.num_vertices) - int(mesh.num_edges) + int(mesh.num_facets)
    )

    valence_id = lagrange.compute_vertex_valence(mesh)
    extended["num_isolated_vertices"] = int(np.sum(mesh.attribute(valence_id).data == 0))

    if mesh.dimension == 3:
        if mesh.is_triangle_mesh:
            mesh_to_check = mesh
        else:
            mesh_to_check = mesh.clone()
            lagrange.triangulate_polygonal_facets(mesh_to_check)
        extended["num_intersecting_pairs"] = int(
            len(lagrange.bvh.compute_intersecting_pairs(mesh_to_check))
        )

    info["extended"] = extended


def print_extra_info(mesh, info: dict) -> None:
    collect_extended_info(mesh, info)
    extended = info["extended"]
    print_property("num components", extended["num_components"])
    print_property("closed", extended["closed"], True)
    if not extended["closed"]:
        print_property("num boundary edges", extended["num_boundary_edges"], 0)
        print_property("num boundary loops", extended["num_boundary_loops"], 0)
    print_property("manifold", extended["manifold"], True)
    if not extended["vertex_manifold"]:
        print_property("non-manifold vertices", extended["nonmanifold_vertices"], 0)
    else:
        print_property("vertex manifold", True, True)
    if not extended["edge_manifold"]:
        print_property("non-manifold edges", extended["nonmanifold_edges"], 0)
    else:
        print_property("edge manifold", True, True)
    if not extended["orientable"]:
        print_property("non-oriented edges", extended["nonoriented_edges"], 0)
    else:
        print_property("orientable", True, True)
    print_property("num degenerate facets", extended["num_degenerate_facets"], 0)
    print_property("num isolated vertices", extended["num_isolated_vertices"], 0)
    print_property("euler characteristic", extended["euler_characteristic"])
    if "num_intersecting_pairs" in extended:
        print_property("num intersecting pairs", extended["num_intersecting_pairs"], 0)


# ---------------------------------------------------------------------------
# Attributes section
# ---------------------------------------------------------------------------


def _usage_to_str(usage) -> str:
    return str(usage).split(".")[-1]


def _element_to_str(element) -> str:
    return str(element).split(".")[-1]


def _dtype_to_str(dtype) -> str:
    try:
        return str(np.dtype(dtype))
    except TypeError:
        return str(dtype)


def collect_attributes(mesh, info: dict) -> None:
    """Populate ``info["attributes"]`` with the list of user-visible attributes."""
    entries: list = []
    for attr_id in mesh.get_matching_attribute_ids():
        name = mesh.get_attribute_name(attr_id)
        if name.startswith("@"):
            continue
        is_indexed = mesh.is_attribute_indexed(attr_id)
        if is_indexed:
            attr = mesh.indexed_attribute(attr_id)
            dtype_str = _dtype_to_str(attr.values.dtype)
        else:
            attr = mesh.attribute(attr_id)
            dtype_str = _dtype_to_str(attr.dtype)
        entries.append(
            {
                "name": name,
                "id": int(attr_id),
                "usage": _usage_to_str(attr.usage),
                "element": _element_to_str(attr.element_type),
                "channels": int(attr.num_channels),
                "dtype": dtype_str,
                "indexed": bool(is_indexed),
            }
        )
    info["attributes"] = entries


def print_attributes(mesh, info: dict) -> None:
    collect_attributes(mesh, info)
    for entry in info["attributes"]:
        name = entry["name"]
        print(f"Attribute {colorama.Fore.GREEN}{name}{colorama.Style.RESET_ALL} ({entry['dtype']})")
        print(
            f"  id:{entry['id']:<5}usage: {entry['usage']:<10}"
            f"elem: {entry['element']:<10}channels: {entry['channels']}"
        )


# ---------------------------------------------------------------------------
# UV section
# ---------------------------------------------------------------------------


def _safe_attribute_name(name: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in name)


def _delete_if_exists(mesh, name: str) -> None:
    if mesh.has_attribute(name):
        mesh.delete_attribute(name)


@contextmanager
def _temp_mesh_attribute(mesh, name_hint: str):
    """Reserve a unique attribute name; delete the attribute on exit if present."""
    name = lagrange.get_unique_attribute_name(mesh, name_hint, emit_warning=False)
    try:
        yield name
    finally:
        _delete_if_exists(mesh, name)


def _normalize_uv_attribute(mesh, uv_attr_id: int) -> tuple[int, str]:
    """Ensure the UV attribute is indexed and float64. Returns (id, name)."""
    if not mesh.is_attribute_indexed(uv_attr_id):
        uv_attr_id = lagrange.map_attribute_in_place(
            mesh, uv_attr_id, lagrange.AttributeElement.Indexed
        )
    if mesh.indexed_attribute(uv_attr_id).values.dtype != np.float64:
        uv_attr_id = lagrange.cast_attribute(mesh, uv_attr_id, np.float64)
    return int(uv_attr_id), mesh.get_attribute_name(uv_attr_id)


def collect_uv_info(mesh, info: dict, metrics: list) -> None:
    """Populate ``info["uv"]`` with per-UV-attribute metrics (charts/flips/overlap/seams/distortion).

    Assumes ``mesh`` is already a triangle mesh.
    """
    info.setdefault("uv", {})

    uv_ids = list(mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV))
    if not uv_ids:
        logger.warning("No UV attributes on mesh; skipping UV section.")
        return

    max_extent = float(info.get("basic", {}).get("max_extent", 0.0))

    with ExitStack() as outer_stack:
        edge_lengths_id: int | None = None

        for uv_attr_id in uv_ids:
            original_name = mesh.get_attribute_name(uv_attr_id)
            safe = _safe_attribute_name(original_name)
            norm_id, compute_name = _normalize_uv_attribute(mesh, uv_attr_id)

            with ExitStack() as inner_stack:
                if compute_name != original_name:
                    inner_stack.callback(_delete_if_exists, mesh, compute_name)

                num_facets = int(mesh.num_facets)
                entry: dict = {"num_facets_evaluated": num_facets}

                chart_attr = inner_stack.enter_context(
                    _temp_mesh_attribute(mesh, f"@meshstat_{safe}_chart_id")
                )
                entry["num_charts"] = int(
                    lagrange.compute_uv_charts(
                        mesh,
                        uv_attribute_name=compute_name,
                        output_attribute_name=chart_attr,
                    )
                )

                orient_attr = inner_stack.enter_context(
                    _temp_mesh_attribute(mesh, f"@meshstat_{safe}_uv_orientation")
                )
                orient = lagrange.compute_uv_orientation(
                    mesh,
                    uv_attribute_name=compute_name,
                    output_attribute_name=orient_attr,
                )
                entry["num_flipped_facets"] = int(orient.negative)
                entry["num_degenerate_facets"] = int(orient.degenerate)
                entry["fraction_flipped_facets"] = (
                    float(orient.negative) / num_facets if num_facets > 0 else 0.0
                )

                overlap_result = lagrange.bvh.compute_uv_overlap(
                    mesh,
                    uv_attribute_name=compute_name,
                    compute_overlap_area=True,
                    compute_overlapping_pairs=True,
                    compute_overlap_coloring=False,
                )
                entry["overlap"] = {
                    "has_overlap": bool(overlap_result.has_overlap),
                    "overlap_area": float(overlap_result.overlap_area)
                    if overlap_result.overlap_area is not None
                    else 0.0,
                    "num_overlapping_pairs": int(len(overlap_result.overlapping_pairs)),
                }

                # Seams (need 3D edge lengths once). Boundary edges are also
                # counted as seams: they bound the UV chart even though they only
                # have UV indices on one side.
                seam_attr = inner_stack.enter_context(
                    _temp_mesh_attribute(mesh, f"@meshstat_{safe}_seam_edges")
                )
                seam_id = lagrange.compute_seam_edges(
                    mesh,
                    norm_id,
                    output_attribute_name=seam_attr,
                    include_boundary_edges=True,
                )
                if edge_lengths_id is None:
                    edge_lengths_attr = outer_stack.enter_context(
                        _temp_mesh_attribute(mesh, "@meshstat_edge_lengths")
                    )
                    edge_lengths_id = lagrange.compute_edge_lengths(
                        mesh, output_attribute_name=edge_lengths_attr
                    )
                seam_mask = np.asarray(mesh.attribute(seam_id).data) != 0
                edge_lengths = np.asarray(mesh.attribute(edge_lengths_id).data)
                total_seam_length = float(edge_lengths[seam_mask].sum()) if seam_mask.any() else 0.0
                entry["seams"] = {
                    "num_seam_edges": int(seam_mask.sum()),
                    "total_length_3d": total_seam_length,
                    "relative_length_3d": (
                        total_seam_length / max_extent if max_extent > 0 else None
                    ),
                }

                distortion: dict = {}
                for metric in metrics:
                    metric_name = str(metric).split(".")[-1]
                    out_attr = inner_stack.enter_context(
                        _temp_mesh_attribute(mesh, f"@meshstat_{safe}_uv_distortion_{metric_name}")
                    )
                    attr_id = lagrange.compute_uv_distortion(mesh, compute_name, out_attr, metric)
                    values = np.asarray(mesh.attribute(attr_id).data, dtype=np.float64)
                    distortion[metric_name] = compute_stats(values)
                entry["distortion"] = distortion

                info["uv"][original_name] = entry


def print_uv_info(info: dict) -> None:
    if not info.get("uv"):
        return
    print_section("UV information")
    for name, entry in info["uv"].items():
        print_property(f"{name}: num charts", entry["num_charts"])
        print_property(f"{name}: num flipped facets", entry["num_flipped_facets"], 0)
        print_property(f"{name}: num degenerate facets", entry["num_degenerate_facets"], 0)
        overlap = entry["overlap"]
        print_property(f"{name}: has overlap", overlap["has_overlap"], False)
        print_property(f"{name}: overlap area", overlap["overlap_area"], 0.0)
        seams = entry["seams"]
        print_property(f"{name}: num seam edges", seams["num_seam_edges"])
        print_property(f"{name}: seam length (3D)", f"{seams['total_length_3d']:.6f}")
        rel = seams["relative_length_3d"]
        print_property(
            f"{name}: seam length (relative)",
            f"{rel:.6f}" if rel is not None else "n/a",
        )
        for metric_name, stats in entry["distortion"].items():
            if stats["min"] is None:
                line = "  no finite values"
            else:
                line = (
                    f"  min={stats['min']:.4g} mean={stats['mean']:.4g} "
                    f"median={stats['median']:.4g} p99={stats['percentiles']['99']:.4g} "
                    f"max={stats['max']:.4g}"
                )
            print(f"{name}: {metric_name} distortion")
            print(line)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


_DISTORTION_METRIC_NAMES = [
    "Dirichlet",
    "InverseDirichlet",
    "SymmetricDirichlet",
    "AreaRatio",
    "MIPS",
]


def parse_args(argv: list | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Print basic information about a mesh file.")
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
        "--uv",
        "-u",
        action="store_true",
        help="run the comprehensive UV statistics section",
    )
    parser.add_argument(
        "--uv-metric",
        action="append",
        choices=_DISTORTION_METRIC_NAMES,
        help=(
            "distortion metric(s) to evaluate when --uv is set "
            "(repeatable; defaults to MIPS and SymmetricDirichlet)"
        ),
    )
    parser.add_argument(
        "--export",
        "-e",
        action="store_true",
        help="export stats into a .json file next to the input mesh",
    )
    parser.add_argument(
        "--stitched",
        "-s",
        action="store_true",
        help=(
            "weld coincident vertices on load (useful for glTF meshes where "
            "chart borders are vertex-duplicated, which otherwise hides seams)"
        ),
    )
    parser.add_argument("input_mesh", help="input mesh file")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> int:
    metric_names = args.uv_metric or ["MIPS", "SymmetricDirichlet"]
    metrics = [getattr(lagrange.DistortionMetric, name) for name in metric_names]

    mesh = lagrange.io.load_mesh(args.input_mesh, quiet=True, stitch_vertices=args.stitched)

    info: dict = {"file": str(args.input_mesh)}

    header = f"Summary of {args.input_mesh}"
    print_header(f"{header:=^55}")
    print_basic_info(mesh, info)

    if args.extended:
        print_extra_info(mesh, info)

    if args.attribute:
        print_attributes(mesh, info)

    if args.uv:
        if not mesh.is_triangle_mesh:
            logger.warning("Triangulating in place for UV stats.")
            if mesh.has_edges:
                mesh.clear_edges()
            lagrange.triangulate_polygonal_facets(mesh)
        collect_uv_info(mesh, info, metrics)
        print_uv_info(info)

    if args.export:
        out = save_info(args.input_mesh, info)
        print(f"Exported stats to {out}")

    return 0


def main(argv: list | None = None) -> int:
    if platform.system() == "Windows":
        colorama.just_fix_windows_console()
    args = parse_args(argv)
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
