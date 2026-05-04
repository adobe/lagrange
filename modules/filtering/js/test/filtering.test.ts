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

describe("filtering", () => {
  test("meshSmoothing with default options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const vertsBefore = mesh.getNumVertices();
    lagrange.filtering.meshSmoothing(mesh);
    // Smoothing modifies positions but preserves topology
    expect(mesh.getNumVertices()).toBe(vertsBefore);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    mesh.delete();
  });

  test("meshSmoothing with vertex smoothing method", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    lagrange.filtering.meshSmoothing(mesh, { method: "vertexSmoothing" });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    mesh.delete();
  });

  test("meshSmoothing with custom weights", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    lagrange.filtering.meshSmoothing(mesh, {
      curvatureWeight: 0.05,
      gradientWeight: 2,
      gradientModulationScale: 0.5,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    mesh.delete();
  });
});
