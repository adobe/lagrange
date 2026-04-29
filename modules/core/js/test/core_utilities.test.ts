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

/** Helper: create a simple triangle mesh. */
function makeTriangleMesh() {
  const mesh = new lagrange.core.SurfaceMesh(3);
  mesh.addVertex([0, 0, 0]);
  mesh.addVertex([1, 0, 0]);
  mesh.addVertex([0, 1, 0]);
  mesh.addTriangle(0, 1, 2);
  return mesh;
}

/** Helper: generate a sphere for tests needing a real mesh. */
function makeSphere() {
  return lagrange.primitive.generateSphere({ radius: 1 });
}

describe("core utilities", () => {
  test("computeVertexNormal adds normals", () => {
    const mesh = makeSphere();
    lagrange.core.computeVertexNormal(mesh);
    expect(mesh.hasAttribute("@vertex_normal")).toBe(true);
    mesh.delete();
  });

  test("computeVertexNormal with weight type", () => {
    const mesh = makeSphere();
    lagrange.core.computeVertexNormal(mesh, { weightType: "uniform" });
    expect(mesh.hasAttribute("@vertex_normal")).toBe(true);
    mesh.delete();
  });

  test("computeFacetNormal adds facet normals", () => {
    const mesh = makeSphere();
    lagrange.core.computeFacetNormal(mesh);
    expect(mesh.hasAttribute("@facet_normal")).toBe(true);
    mesh.delete();
  });

  test("computeNormal adds indexed normals with feature angle", () => {
    const mesh = makeSphere();
    lagrange.core.computeNormal(mesh, Math.PI / 6);
    expect(mesh.hasAttribute("@normal")).toBe(true);
    mesh.delete();
  });

  test("computeTangentBitangent adds tangent attributes", () => {
    const mesh = makeSphere();
    // Sphere already has UVs and normals from generation
    lagrange.core.computeTangentBitangent(mesh);
    expect(mesh.hasAttribute("@tangent")).toBe(true);
    expect(mesh.hasAttribute("@bitangent")).toBe(true);
    mesh.delete();
  });

  test("triangulatePolygonalFacets is a no-op on triangle mesh", () => {
    const mesh = makeTriangleMesh();
    const numFacets = mesh.getNumFacets();
    lagrange.core.triangulatePolygonalFacets(mesh);
    expect(mesh.getNumFacets()).toBe(numFacets);
    mesh.delete();
  });

  test("combineMeshes merges two meshes", () => {
    const a = makeTriangleMesh();
    const b = makeTriangleMesh();
    const combined = lagrange.core.combineMeshes([a, b]);
    expect(combined.getNumVertices()).toBe(6);
    expect(combined.getNumFacets()).toBe(2);
    combined.delete();
    a.delete();
    b.delete();
  });

  test("computeComponents returns component count", () => {
    const a = makeTriangleMesh();
    const b = makeTriangleMesh();
    const mesh = lagrange.core.combineMeshes([a, b]);
    const count = lagrange.core.computeComponents(mesh);
    expect(count).toBe(2);
    mesh.delete();
    a.delete();
    b.delete();
  });

  test("orientOutward runs without error", () => {
    const mesh = makeSphere();
    lagrange.core.orientOutward(mesh);
    expect(mesh.getNumFacets()).toBeGreaterThan(0);
    mesh.delete();
  });

  test("normalizeMesh scales mesh to unit box", () => {
    const mesh = makeSphere();
    lagrange.core.normalizeMesh(mesh);
    // After normalization, positions should be within [-0.5, 0.5]
    const pos = mesh.getPosition(0);
    expect(Math.abs(pos[0])).toBeLessThanOrEqual(0.6);
    expect(Math.abs(pos[1])).toBeLessThanOrEqual(0.6);
    expect(Math.abs(pos[2])).toBeLessThanOrEqual(0.6);
    mesh.delete();
  });

  test("computeFacetArea adds area attribute", () => {
    const mesh = makeSphere();
    lagrange.core.computeFacetArea(mesh);
    expect(mesh.hasAttribute("@facet_area")).toBe(true);
    mesh.delete();
  });

  test("isManifold returns true for sphere", () => {
    const mesh = makeSphere();
    expect(lagrange.core.isManifold(mesh)).toBe(true);
    mesh.delete();
  });

  test("isVertexManifold returns true for sphere", () => {
    const mesh = makeSphere();
    expect(lagrange.core.isVertexManifold(mesh)).toBe(true);
    mesh.delete();
  });

  test("isEdgeManifold returns true for sphere", () => {
    const mesh = makeSphere();
    expect(lagrange.core.isEdgeManifold(mesh)).toBe(true);
    mesh.delete();
  });

  test("isClosed returns true for sphere", () => {
    const mesh = makeSphere();
    expect(lagrange.core.isClosed(mesh)).toBe(true);
    mesh.delete();
  });

  test("isClosed returns false for single triangle", () => {
    const mesh = makeTriangleMesh();
    expect(lagrange.core.isClosed(mesh)).toBe(false);
    mesh.delete();
  });

  test("computeEuler returns 2 for sphere (V-E+F=2)", () => {
    const mesh = makeSphere();
    expect(lagrange.core.computeEuler(mesh)).toBe(2);
    mesh.delete();
  });

  test("computeVertexValence adds valence attribute", () => {
    const mesh = makeSphere();
    lagrange.core.computeVertexValence(mesh);
    expect(mesh.hasAttribute("@vertex_valence")).toBe(true);
    mesh.delete();
  });

  test("computeGreedyColoring adds color attribute", () => {
    const mesh = makeSphere();
    lagrange.core.computeGreedyColoring(mesh);
    expect(mesh.hasAttribute("@color_id")).toBe(true);
    mesh.delete();
  });

  test("removeDuplicateFacets removes exact duplicates", () => {
    const mesh = makeTriangleMesh();
    // Add same triangle again
    mesh.addTriangle(0, 1, 2);
    expect(mesh.getNumFacets()).toBe(2);
    lagrange.core.removeDuplicateFacets(mesh);
    expect(mesh.getNumFacets()).toBe(1);
    mesh.delete();
  });

  test("removeIsolatedVertices cleans up", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([99, 99, 99]); // isolated
    mesh.addTriangle(0, 1, 2);
    expect(mesh.getNumVertices()).toBe(4);
    lagrange.core.removeIsolatedVertices(mesh);
    expect(mesh.getNumVertices()).toBe(3);
    mesh.delete();
  });

  test("removeNullAreaFacets removes degenerate triangles", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([0, 0, 0]); // collinear with v0
    mesh.addTriangle(0, 1, 2); // valid
    mesh.addTriangle(0, 3, 0); // degenerate (zero area)
    lagrange.core.removeNullAreaFacets(mesh);
    expect(mesh.getNumFacets()).toBe(1);
    mesh.delete();
  });

  test("removeTopologicallyDegenerateFacets runs without error", () => {
    const mesh = makeSphere();
    const facets = mesh.getNumFacets();
    lagrange.core.removeTopologicallyDegenerateFacets(mesh);
    expect(mesh.getNumFacets()).toBe(facets); // sphere has no degenerate facets
    mesh.delete();
  });

  test("resolveNonmanifoldness runs without error", () => {
    const mesh = makeSphere();
    lagrange.core.resolveNonmanifoldness(mesh);
    expect(lagrange.core.isManifold(mesh)).toBe(true);
    mesh.delete();
  });

  test("flipFacets reverses winding", () => {
    const mesh = makeTriangleMesh();
    const v0 = mesh.getFacetVertex(0, 0);
    const v1 = mesh.getFacetVertex(0, 1);
    const v2 = mesh.getFacetVertex(0, 2);
    mesh.flipFacets();
    // After flip, winding is reversed: [0,1,2] → [2,1,0]
    expect(mesh.getFacetVertex(0, 0)).toBe(v2);
    expect(mesh.getFacetVertex(0, 1)).toBe(v1);
    expect(mesh.getFacetVertex(0, 2)).toBe(v0);
    mesh.delete();
  });

  test("addVertices adds multiple vertices at once", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertices([0, 0, 0, 1, 0, 0, 0, 1, 0]);
    expect(mesh.getNumVertices()).toBe(3);
    mesh.delete();
  });

  test("removeVertices removes specified vertices", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertices([0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]);
    mesh.addTriangle(0, 1, 2);
    expect(mesh.getNumVertices()).toBe(4);
    mesh.removeVertices([3]); // remove isolated vertex
    expect(mesh.getNumVertices()).toBe(3);
    mesh.delete();
  });

  test("removeFacets removes specified facets", () => {
    const a = makeTriangleMesh();
    const b = makeTriangleMesh();
    const mesh = lagrange.core.combineMeshes([a, b]);
    expect(mesh.getNumFacets()).toBe(2);
    mesh.removeFacets([0]);
    expect(mesh.getNumFacets()).toBe(1);
    mesh.delete();
    a.delete();
    b.delete();
  });
});
