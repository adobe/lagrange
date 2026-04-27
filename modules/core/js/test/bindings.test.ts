import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("bindings", () => {
  test("meshToBabylonMeshData32 returns positions, normals, uvs and indices", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const data = lagrange.bindings.meshToBabylonMeshData32(mesh);
    mesh.delete();
    expect(data.numVertices).toBeGreaterThan(0);
    expect(data.numTriangles).toBeGreaterThan(0);
    expect(data.positions.length).toBe(data.numVertices * 3);
    expect(data.indices.length).toBe(data.numTriangles * 3);
    expect(data.normals).toBeDefined();
    expect(data.normals!.length).toBe(data.numVertices * 3);
    expect(data.uvs).toBeDefined();
    expect(data.uvs!.length).toBe(data.numVertices * 2);
  });

  test("meshToBabylonMeshDataView64 returns views with normals and uvs", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const unified = lagrange.core.unifyIndexBuffer(mesh);
    const data = lagrange.bindings.meshToBabylonMeshDataView64(unified);
    expect(data).toBeDefined();
    expect(data!.numVertices).toBeGreaterThan(0);
    expect(data!.numTriangles).toBeGreaterThan(0);
    expect(data!.positions.length).toBe(data!.numVertices * 3);
    expect(data!.indices.length).toBe(data!.numTriangles * 3);
    expect(data!.normals).toBeDefined();
    expect(data!.normals!.length).toBe(data!.numVertices * 3);
    expect(data!.uvs).toBeDefined();
    expect(data!.uvs!.length).toBe(data!.numVertices * 2);
    unified.delete();
    mesh.delete();
  });
});
