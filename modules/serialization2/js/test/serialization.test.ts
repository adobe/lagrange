import { describe, test, expect, beforeAll } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange } from "../src/types.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

describe("serialization", () => {
  test("round-trip: serialize then deserialize preserves mesh", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const numVerts = mesh.getNumVertices();
    const numFacets = mesh.getNumFacets();

    const data = lagrange.serialization.serializeMesh(mesh);
    expect(data.length).toBeGreaterThan(0);

    const restored = lagrange.serialization.deserializeMesh(data);
    expect(restored.getNumVertices()).toBe(numVerts);
    expect(restored.getNumFacets()).toBe(numFacets);

    restored.delete();
    mesh.delete();
  });

  test("serializeMesh with compression options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });

    const compressed = lagrange.serialization.serializeMesh(mesh, { compress: true, compressionLevel: 10 });
    const uncompressed = lagrange.serialization.serializeMesh(mesh, { compress: false });

    expect(compressed.length).toBeLessThan(uncompressed.length);

    mesh.delete();
  });

  test("deserializeMesh with options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const data = lagrange.serialization.serializeMesh(mesh);
    const restored = lagrange.serialization.deserializeMesh(data, { quiet: true });
    expect(restored.getNumVertices()).toBe(mesh.getNumVertices());
    restored.delete();
    mesh.delete();
  });

  test("round-trip: serialize then deserialize preserves scene", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const scene = lagrange.scene.meshToScene(mesh);
    const numMeshes = scene.getNumMeshes();
    const numNodes = scene.getNumNodes();

    const data = lagrange.serialization.serializeScene(scene);
    expect(data.length).toBeGreaterThan(0);

    const restored = lagrange.serialization.deserializeScene(data);
    expect(restored.getNumMeshes()).toBe(numMeshes);
    expect(restored.getNumNodes()).toBe(numNodes);

    restored.delete();
    scene.delete();
    mesh.delete();
  });

  test("serializeScene with compression options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const scene = lagrange.scene.meshToScene(mesh);

    const compressed = lagrange.serialization.serializeScene(scene, { compress: true, compressionLevel: 10 });
    const uncompressed = lagrange.serialization.serializeScene(scene, { compress: false });

    expect(compressed.length).toBeLessThan(uncompressed.length);

    scene.delete();
    mesh.delete();
  });

  test("deserializeScene with options", () => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const scene = lagrange.scene.meshToScene(mesh);
    const data = lagrange.serialization.serializeScene(scene);
    const restored = lagrange.serialization.deserializeScene(data, { quiet: true });
    expect(restored.getNumMeshes()).toBe(scene.getNumMeshes());
    restored.delete();
    scene.delete();
    mesh.delete();
  });
});
