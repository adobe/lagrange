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
"""Subprocess smoke tests for the installed ``lagrange.scripts.meshstat`` module."""

import json
import subprocess
import sys

import lagrange


class TestMeshstatSmoke:
    def test_help(self):
        result = subprocess.run(
            [sys.executable, "-m", "lagrange.scripts.meshstat", "--help"],
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "usage:" in result.stdout.lower()

    def test_export_uv(self, cube_with_uv, tmp_path):
        mesh_path = tmp_path / "cube.obj"
        lagrange.io.save_mesh(str(mesh_path), cube_with_uv)
        result = subprocess.run(
            [sys.executable, "-m", "lagrange.scripts.meshstat", "--uv", "--export", str(mesh_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        json_path = mesh_path.with_suffix(".json")
        assert json_path.exists()
        info = json.loads(json_path.read_text())
        assert "basic" in info
        assert "uv" in info
