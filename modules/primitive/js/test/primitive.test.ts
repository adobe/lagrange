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

describe("primitive", () => {
  test("generateSphere with all options", () => {
    const mesh = lagrange.primitive.generateSphere({
      radius: 1,
      numLongitudeSections: 8,
      numLatitudeSections: 8,
      startSweepAngle: 0,
      endSweepAngle: Math.PI * 2,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateSphere with partial options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 2 });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateSphere with empty options uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateSphere({});
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateSphere with no arguments uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateSphere();
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateTorus with partial options", () => {
    const mesh = lagrange.primitive.generateTorus({ majorRadius: 3 });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateTorus with empty options uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateTorus({});
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateRoundedCube with partial options", () => {
    const mesh = lagrange.primitive.generateRoundedCube({
      width: 2,
      height: 2,
      depth: 2,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateRoundedCube with empty options uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateRoundedCube({});
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateRoundedCone with partial options", () => {
    const mesh = lagrange.primitive.generateRoundedCone({
      radiusBottom: 2,
      height: 3,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateDisc with partial options", () => {
    const mesh = lagrange.primitive.generateDisc({ radius: 5 });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateRoundedPlane with partial options", () => {
    const mesh = lagrange.primitive.generateRoundedPlane({
      width: 4,
      height: 3,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateIcosahedron produces 12 vertices and 20 faces", () => {
    const mesh = lagrange.primitive.generateIcosahedron({ radius: 1 });
    expect(mesh.getNumVertices()).toBe(12);
    expect(mesh.getNumFacets()).toBe(20);
    mesh.delete();
  });

  test("generateIcosahedron with empty options uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateIcosahedron({});
    expect(mesh.getNumVertices()).toBe(12);
    expect(mesh.getNumFacets()).toBe(20);
    mesh.delete();
  });

  test("generateOctahedron with empty options uses C++ defaults", () => {
    const mesh = lagrange.primitive.generateOctahedron({});
    expect(mesh.getNumVertices()).toBe(6);
    expect(mesh.getNumFacets()).toBe(8);
    mesh.delete();
  });

  test("center option offsets mesh position", () => {
    const mesh = lagrange.primitive.generateSphere({
      radius: 1,
      center: [10, 20, 30],
    });
    // Check first vertex is near center offset
    const pos = mesh.getPosition(0);
    expect(pos[0]).toBeGreaterThan(8);
    expect(pos[1]).toBeGreaterThan(18);
    expect(pos[2]).toBeGreaterThan(28);
    mesh.delete();
  });

  test("generateSubdividedSphere with default options", () => {
    const mesh = lagrange.primitive.generateSubdividedSphere({});
    expect(mesh.getNumVertices()).toBe(12); // base icosahedron, subdiv 0
    expect(mesh.getNumFacets()).toBe(20);
    mesh.delete();
  });

  test("generateSubdividedSphere with subdivision", () => {
    const mesh = lagrange.primitive.generateSubdividedSphere({
      subdivLevel: 2,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(12);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("generateSweptSurface circular sweep", () => {
    // Simple square profile swept around Y axis
    const profile = [0, 0, 1, 0, 1, 1, 0, 1];
    const mesh = lagrange.primitive.generateSweptSurface(profile, {
      type: "circular",
      point: [1, 0, 0],
      axis: [0, 1, 0],
      numSamples: 16,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    mesh.delete();
  });

  test("generateSweptSurface linear sweep", () => {
    const profile = [0, 0, 1, 0, 1, 1, 0, 1];
    const mesh = lagrange.primitive.generateSweptSurface(profile, {
      type: "linear",
      from: [0, 0, 0],
      to: [0, 2, 0],
      numSamples: 4,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    mesh.delete();
  });

  test("generateSweptSurface with taper function", () => {
    const profile = [0, 0, 1, 0, 1, 1, 0, 1];
    const mesh = lagrange.primitive.generateSweptSurface(profile, {
      type: "linear",
      from: [0, 0, 0],
      to: [0, 3, 0],
      numSamples: 8,
      taperFunction: (t: number) => 1 - t * 0.5,
    });
    expect(mesh.getNumVertices()).toBeGreaterThan(0);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    mesh.delete();
  });
});
