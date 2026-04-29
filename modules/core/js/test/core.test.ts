/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";
import { invalidScalar, invalidIndex } from "../src/modules/core.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("core", () => {
  test("invalidScalar is Infinity, invalidIndex is 0xFFFFFFFF", () => {
    expect(invalidScalar).toBe(Infinity);
    expect(invalidIndex).toBe(0xFFFFFFFF);
  });

  test("removeDuplicateVertices merges coincident vertices", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    // Two triangles sharing an edge but with duplicate vertices
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([0, 0, 0]); // duplicate of v0
    mesh.addVertex([1, 0, 0]); // duplicate of v1
    mesh.addVertex([0, -1, 0]);
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(3, 5, 4);
    expect(mesh.getNumVertices()).toBe(6);
    lagrange.core.removeDuplicateVertices(mesh);
    expect(mesh.getNumVertices()).toBe(4);
    mesh.delete();
  });

  test("unifyIndexBuffer returns a mesh", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    const unified = lagrange.core.unifyIndexBuffer(mesh);
    expect(unified.getNumFacets()).toBe(1);
    unified.delete();
    mesh.delete();
  });
});
