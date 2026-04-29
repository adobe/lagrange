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

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("subdivision", () => {
  test("subdivideMesh with Catmull-Clark on quad mesh", () => {
    const mesh = lagrange.primitive.generateRoundedCube({ triangulate: false });
    const subdivided = lagrange.subdivision.subdivideMesh(mesh, {
      scheme: "catmullClark",
      numLevels: 1,
    });
    expect(subdivided.getNumVertices()).toBeGreaterThan(mesh.getNumVertices());
    expect(subdivided.getNumFacets()).toBeGreaterThan(mesh.getNumFacets());
    subdivided.delete();
    mesh.delete();
  });

  test("subdivideMesh with Loop on triangle mesh", () => {
    const mesh = lagrange.primitive.generateSphere();
    const subdivided = lagrange.subdivision.subdivideMesh(mesh, {
      scheme: "loop",
      numLevels: 1,
    });
    expect(subdivided.getNumVertices()).toBeGreaterThan(mesh.getNumVertices());
    expect(subdivided.isTriangleMesh()).toBe(true);
    subdivided.delete();
    mesh.delete();
  });

  test("subdivideMesh with default options", () => {
    const mesh = lagrange.primitive.generateSphere();
    const subdivided = lagrange.subdivision.subdivideMesh(mesh, {});
    expect(subdivided.getNumVertices()).toBeGreaterThan(mesh.getNumVertices());
    subdivided.delete();
    mesh.delete();
  });

  test("midpointSubdivision increases vertex count", () => {
    const mesh = lagrange.primitive.generateIcosahedron({});
    const subdivided = lagrange.subdivision.midpointSubdivision(mesh);
    expect(subdivided.getNumVertices()).toBeGreaterThan(12);
    subdivided.delete();
    mesh.delete();
  });

  test("sqrtSubdivision on triangle mesh", () => {
    const mesh = lagrange.primitive.generateIcosahedron({});
    const subdivided = lagrange.subdivision.sqrtSubdivision(mesh);
    expect(subdivided.getNumVertices()).toBeGreaterThan(12);
    expect(subdivided.isTriangleMesh()).toBe(true);
    subdivided.delete();
    mesh.delete();
  });
});
