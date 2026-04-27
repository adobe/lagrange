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
