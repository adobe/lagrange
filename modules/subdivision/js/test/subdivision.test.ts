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
