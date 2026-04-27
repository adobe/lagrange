import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("bvh", () => {
  test("computeHausdorff returns 0 for identical meshes", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const dist = lagrange.bvh.computeHausdorff(mesh, mesh);
    expect(dist).toBeCloseTo(0, 5);
    mesh.delete();
  });

  test("computeHausdorff returns nonzero for different meshes", () => {
    const a = lagrange.primitive.generateSphere({ radius: 1 });
    const b = lagrange.primitive.generateSphere({ radius: 2 });
    const dist = lagrange.bvh.computeHausdorff(a, b);
    expect(dist).toBeGreaterThan(0.5);
    a.delete();
    b.delete();
  });

  test("computeChamfer returns 0 for identical meshes", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const dist = lagrange.bvh.computeChamfer(mesh, mesh);
    expect(dist).toBeCloseTo(0, 5);
    mesh.delete();
  });

  test("computeMeshDistances adds distance attribute", () => {
    const source = lagrange.primitive.generateSphere({ radius: 1 });
    const target = lagrange.primitive.generateSphere({ radius: 2 });
    lagrange.bvh.computeMeshDistances(source, target);
    expect(source.hasAttribute("@distance_to_mesh")).toBe(true);
    source.delete();
    target.delete();
  });

  test("weldVertices merges nearby vertices", () => {
    // Create two triangles with nearly-duplicate vertices
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([0, 1e-8, 0]);     // near-duplicate of v0
    mesh.addVertex([1, 1e-8, 0]);     // near-duplicate of v1
    mesh.addVertex([0, -1, 0]);
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(3, 5, 4);
    expect(mesh.getNumVertices()).toBe(6);
    lagrange.bvh.weldVertices(mesh, { radius: 1e-6 });
    expect(mesh.getNumVertices()).toBe(4);
    mesh.delete();
  });
});
