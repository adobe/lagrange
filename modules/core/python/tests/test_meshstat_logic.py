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
import json
import logging

import lagrange
import lagrange.scripts.meshstat as meshstat
import numpy as np
import pytest


class TestComputeStats:
    def test_empty(self):
        stats = meshstat.compute_stats(np.array([], dtype=np.float64))
        assert stats["count"] == 0
        assert stats["num_invalid"] == 0
        assert stats["min"] is None
        assert stats["percentiles"] == {}

    def test_all_finite(self):
        stats = meshstat.compute_stats(np.array([1.0, 2.0, 3.0, 4.0, 5.0]))
        assert stats["count"] == 5
        assert stats["num_invalid"] == 0
        assert stats["min"] == 1.0
        assert stats["max"] == 5.0
        assert stats["mean"] == pytest.approx(3.0)
        assert stats["median"] == pytest.approx(3.0)
        for key in ("1", "10", "25", "75", "90", "99"):
            assert key in stats["percentiles"]

    def test_mixed_invalid(self):
        values = np.array([1.0, 2.0, np.inf, np.nan, -np.inf, 3.0])
        stats = meshstat.compute_stats(values)
        assert stats["count"] == 6
        assert stats["num_invalid"] == 3
        assert stats["min"] == 1.0
        assert stats["max"] == 3.0
        assert stats["mean"] == pytest.approx(2.0)

    def test_only_invalid(self):
        stats = meshstat.compute_stats(np.array([np.nan, np.inf, -np.inf]))
        assert stats["count"] == 3
        assert stats["num_invalid"] == 3
        assert stats["min"] is None


class TestCollectBasicInfo:
    def test_cube_with_uv(self, cube_with_uv):
        info: dict = {}
        meshstat.collect_basic_info(cube_with_uv, info)
        basic = info["basic"]
        assert basic["num_vertices"] == 8
        assert basic["num_facets"] == 6
        assert basic["facet_type"] == "quads"
        assert basic["dim"] == 3
        assert basic["bbox_min"] == [0.0, 0.0, 0.0]
        assert basic["bbox_max"] == [1.0, 1.0, 1.0]
        assert basic["bbox_extent"] == [1.0, 1.0, 1.0]
        assert basic["max_extent"] == 1.0
        assert basic["bbox_diagonal"] == pytest.approx(np.sqrt(3))

    def test_initializes_edges(self, cube_with_uv):
        """``num_edges`` must be reported correctly even when the caller has
        not initialized edges beforehand."""
        assert not cube_with_uv.has_edges
        info: dict = {}
        meshstat.collect_basic_info(cube_with_uv, info)
        assert cube_with_uv.has_edges
        assert info["basic"]["num_edges"] == 12

    def test_empty_mesh(self, capsys):
        """An empty mesh must not crash bbox computation, and ``print_basic_info``
        must still emit a sensible summary (so ``--export`` works on failed loads)."""
        mesh = lagrange.SurfaceMesh()
        info: dict = {}
        meshstat.print_basic_info(mesh, info)
        basic = info["basic"]
        assert basic["num_vertices"] == 0
        assert basic["bbox_min"] is None
        assert basic["bbox_max"] is None
        assert basic["bbox_extent"] is None
        assert basic["bbox_diagonal"] == 0.0
        assert basic["max_extent"] == 0.0
        assert "empty mesh" in capsys.readouterr().out


class TestCollectUVInfo:
    def test_triangulated_cube(self, cube_with_uv):
        mesh = cube_with_uv
        mesh.initialize_edges()
        lagrange.triangulate_polygonal_facets(mesh)
        info: dict = {"basic": {"max_extent": 1.0}}
        metrics = [lagrange.DistortionMetric.MIPS, lagrange.DistortionMetric.SymmetricDirichlet]
        meshstat.collect_uv_info(mesh, info, metrics)

        assert "uv" in info["uv"]
        uv_info = info["uv"]["uv"]
        assert uv_info["num_facets_evaluated"] == mesh.num_facets
        assert uv_info["num_charts"] >= 1
        assert uv_info["num_flipped_facets"] == 0
        assert uv_info["num_degenerate_facets"] == 0
        assert uv_info["fraction_flipped_facets"] == 0.0
        overlap = uv_info["overlap"]
        assert overlap["has_overlap"] is False
        assert overlap["overlap_area"] == 0.0
        assert overlap["num_overlapping_pairs"] == 0
        seams = uv_info["seams"]
        assert seams["num_seam_edges"] >= 0
        assert seams["total_length_3d"] >= 0.0
        assert seams["relative_length_3d"] is not None
        for metric_name in ("MIPS", "SymmetricDirichlet"):
            stats = uv_info["distortion"][metric_name]
            for key in (
                "count",
                "num_invalid",
                "min",
                "max",
                "mean",
                "median",
                "std",
                "percentiles",
            ):
                assert key in stats
            assert stats["count"] == mesh.num_facets

    def test_no_uv_warns(self, cube, caplog):
        cube.initialize_edges()
        lagrange.triangulate_polygonal_facets(cube)
        info: dict = {}
        with caplog.at_level(logging.WARNING, logger=meshstat.__name__):
            meshstat.collect_uv_info(cube, info, [lagrange.DistortionMetric.MIPS])
        assert info["uv"] == {}
        assert any("No UV attributes" in rec.message for rec in caplog.records)

    def test_idempotent(self, cube_with_uv):
        """Calling ``collect_uv_info`` twice on the same mesh must not crash:
        intermediate attributes should get unique names instead of colliding."""
        mesh = cube_with_uv
        mesh.initialize_edges()
        lagrange.triangulate_polygonal_facets(mesh)
        info: dict = {"basic": {"max_extent": 1.0}}
        metrics = [lagrange.DistortionMetric.MIPS]
        meshstat.collect_uv_info(mesh, info, metrics)
        first = dict(info["uv"]["uv"])
        info["uv"] = {}
        meshstat.collect_uv_info(mesh, info, metrics)
        second = info["uv"]["uv"]
        assert first["num_charts"] == second["num_charts"]
        assert first["seams"]["num_seam_edges"] == second["seams"]["num_seam_edges"]

    def test_open_boundary_counts_as_seams(self):
        """A flat square (open boundary, single chart, no UV cut) must report
        the four perimeter edges as seams, since boundary edges bound the
        UV chart even when ``compute_seam_edges`` does not flag them."""
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], dtype=np.float64))
        mesh.add_triangles(np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32))
        mesh.create_attribute(
            "uv",
            lagrange.AttributeElement.Indexed,
            lagrange.AttributeUsage.UV,
            np.array([[0, 0], [1, 0], [1, 1], [0, 1]], dtype=np.float32),
            np.array([0, 1, 2, 0, 2, 3], dtype=np.uint32),
        )
        mesh.initialize_edges()
        info: dict = {"basic": {"max_extent": 1.0}}
        meshstat.collect_uv_info(mesh, info, [])
        seams = info["uv"]["uv"]["seams"]
        assert seams["num_seam_edges"] == 4
        assert seams["total_length_3d"] == pytest.approx(4.0)


class TestMain:
    def _save_mesh(self, mesh, path):
        lagrange.io.save_mesh(str(path), mesh)

    def test_export_writes_nested_json(self, cube_with_uv, tmp_path):
        mesh_path = tmp_path / "cube.obj"
        self._save_mesh(cube_with_uv, mesh_path)
        rc = meshstat.main(["--export", "--extended", str(mesh_path)])
        assert rc == 0
        json_path = mesh_path.with_suffix(".json")
        assert json_path.exists()
        info = json.loads(json_path.read_text())
        assert "basic" in info and "extended" in info
        assert info["basic"]["num_vertices"] == cube_with_uv.num_vertices

    def test_uv_export_schema(self, cube_with_uv, tmp_path):
        mesh_path = tmp_path / "cube.obj"
        self._save_mesh(cube_with_uv, mesh_path)
        rc = meshstat.main(["--uv", "--export", str(mesh_path)])
        assert rc == 0
        info = json.loads(mesh_path.with_suffix(".json").read_text())
        assert "uv" in info
        for uv_entry in info["uv"].values():
            assert {"num_charts", "overlap", "seams", "distortion"} <= set(uv_entry.keys())

    def test_uv_triangulates_in_place_with_warning(self, cube_with_uv, tmp_path, caplog):
        mesh_path = tmp_path / "cube.obj"
        self._save_mesh(cube_with_uv, mesh_path)
        with caplog.at_level(logging.WARNING, logger=meshstat.__name__):
            rc = meshstat.main(["--uv", str(mesh_path)])
        assert rc == 0
        triangulate_records = [
            rec for rec in caplog.records if "triangulating in place" in rec.message.lower()
        ]
        assert len(triangulate_records) == 1
