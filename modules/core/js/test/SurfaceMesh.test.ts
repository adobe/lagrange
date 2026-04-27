import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("SurfaceMesh", () => {
  test("create empty mesh", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    expect(mesh.getNumVertices()).toBe(0);
    expect(mesh.getNumFacets()).toBe(0);
    expect(mesh.getDimension()).toBe(3);
    mesh.delete();
  });

  test("add vertices and triangles", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    expect(mesh.getNumVertices()).toBe(3);
    expect(mesh.getNumFacets()).toBe(1);
    expect(mesh.isTriangleMesh()).toBe(true);
    mesh.delete();
  });

  test("getPosition returns vertex coordinates", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([1, 2, 3]);
    const pos = mesh.getPosition(0);
    expect(pos[0]).toBeCloseTo(1);
    expect(pos[1]).toBeCloseTo(2);
    expect(pos[2]).toBeCloseTo(3);
    mesh.delete();
  });

  test("clone produces independent mesh", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);

    const copy = mesh.clone();
    expect(copy.getNumVertices()).toBe(3);
    expect(copy.getNumFacets()).toBe(1);

    // Mutate the clone; original must remain unchanged.
    copy.addVertex([1, 1, 0]);
    copy.addTriangle(1, 3, 2);
    expect(copy.getNumVertices()).toBe(4);
    expect(copy.getNumFacets()).toBe(2);
    expect(mesh.getNumVertices()).toBe(3);
    expect(mesh.getNumFacets()).toBe(1);

    mesh.delete();
    copy.delete();
  });

  test("clone survives deletion of original", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);

    const copy = mesh.clone();
    mesh.delete();

    expect(copy.getNumVertices()).toBe(3);
    expect(copy.getNumFacets()).toBe(1);
    const pos = copy.getPosition(1);
    expect(pos[0]).toBeCloseTo(1);
    copy.addVertex([1, 1, 0]);
    copy.addTriangle(1, 3, 2);
    expect(copy.getNumVertices()).toBe(4);
    expect(copy.getNumFacets()).toBe(2);

    copy.delete();
  });

  test("clone with strip drops user attributes", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    lagrange.core.computeFacetNormal(mesh);
    expect(mesh.hasAttribute("@facet_normal")).toBe(true);

    const stripped = mesh.clone({ strip: true });
    expect(stripped.getNumVertices()).toBe(3);
    expect(stripped.getNumFacets()).toBe(1);
    expect(stripped.hasAttribute("@facet_normal")).toBe(false);

    const kept = mesh.clone();
    expect(kept.hasAttribute("@facet_normal")).toBe(true);

    mesh.delete();
    stripped.delete();
    kept.delete();
  });

  test("flipFacets with no argument flips all facets", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([1, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(1, 3, 2);

    const before0 = Array.from(mesh.getFacetVertices(0));
    const before1 = Array.from(mesh.getFacetVertices(1));
    mesh.flipFacets();
    const after0 = Array.from(mesh.getFacetVertices(0));
    const after1 = Array.from(mesh.getFacetVertices(1));

    expect(after0).toEqual([...before0].reverse());
    expect(after1).toEqual([...before1].reverse());
    mesh.delete();
  });

  test("flipFacets with index list only flips listed facets", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addVertex([1, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(1, 3, 2);

    const before0 = Array.from(mesh.getFacetVertices(0));
    const before1 = Array.from(mesh.getFacetVertices(1));
    mesh.flipFacets([0]);
    expect(Array.from(mesh.getFacetVertices(0))).toEqual([...before0].reverse());
    expect(Array.from(mesh.getFacetVertices(1))).toEqual(before1);
    mesh.delete();
  });

  test("getFacetVertices returns correct indices", () => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertex([0, 0, 0]);
    mesh.addVertex([1, 0, 0]);
    mesh.addVertex([0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    const verts = mesh.getFacetVertices(0);
    expect(Array.from(verts)).toEqual([0, 1, 2]);
    mesh.delete();
  });
});
