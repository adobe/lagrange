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

describe("SimpleScene serialization", () => {
  test.each([true, false])("round-trip preserves meshes, attributes and instances (compress=%s)", (compress) => {
    const mesh = new lagrange.core.SurfaceMesh(3);
    mesh.addVertices([0, 0, 0, 1, 0, 0, 0, 1, 0]);
    mesh.addTriangle(0, 1, 2);
    const weights = new Float32Array([1, 2, 3]);
    mesh.createAttribute("weights", { element: "vertex", usage: "scalar", data: weights });
    mesh.createAttribute("uv", {
      element: "indexed", usage: "uv", numChannels: 2,
      data: new Float32Array([0, 0, 1, 0, 0, 1]), indices: new Uint32Array([0, 1, 2]),
    });
    const scene = lagrange.scene.meshesToSimpleScene([mesh, mesh]);
    scene.addMesh(mesh);
    const transform = new Float64Array([2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1]);
    scene.refInstance(0, 0).transform = transform;
    const instance = new lagrange.scene.MeshInstance();
    instance.meshIndex = 0;
    scene.addInstance(instance);
    instance.delete();
    const data = lagrange.serialization.serializeSimpleScene(scene, {
      compress, compressionLevel: 7, numThreads: 1,
    });
    expect(data).toBeInstanceOf(Uint8Array);
    expect(data.byteLength).toBeGreaterThan(0);
    scene.delete();
    mesh.delete();

    const padded = new Uint8Array(data.length + 8);
    padded.set(data, 4);
    const restored = lagrange.serialization.deserializeSimpleScene(padded.subarray(4, 4 + data.length), {
      quiet: true, allowTypeCast: true,
    });
    padded.fill(0);
    expect(restored.getNumMeshes()).toBe(3);
    expect(restored.computeNumInstances()).toBe(3);
    expect([0, 1, 2].map((i) => restored.getNumInstances(i))).toEqual([2, 1, 0]);
    for (let i = 0; i < 3; ++i) {
      const restoredMesh = restored.refMesh(i);
      expect(restoredMesh.getNumVertices()).toBe(3);
      expect(restoredMesh.getFacetVertices(0)).toEqual(new Uint32Array([0, 1, 2]));
      expect(restoredMesh.getPosition(1)).toEqual(new Float32Array([1, 0, 0]));
      expect(restoredMesh.getAttribute("weights").data).toEqual(weights);
      expect(restoredMesh.getIndexedAttribute("uv").indices).toEqual(new Uint32Array([0, 1, 2]));
      expect(restoredMesh.getIndexedAttribute("uv").values.data).toEqual(new Float32Array([0, 0, 1, 0, 0, 1]));
    }
    expect(restored.refInstance(0, 0).transform).toEqual(transform);
    expect(restored.refInstance(1, 0).meshIndex).toBe(1);
    restored.delete();
  });

  test("round-trip an empty SimpleScene with default options", () => {
    const scene = new lagrange.scene.SimpleScene();
    const restored = lagrange.serialization.deserializeSimpleScene(
      lagrange.serialization.serializeSimpleScene(scene),
    );
    expect(restored.getNumMeshes()).toBe(0);
    expect(restored.computeNumInstances()).toBe(0);
    restored.delete();
    scene.delete();
  });

  test.each(["mesh", "scene"] as const)("conversion to and from %s buffers is opt-in", (kind) => {
    const mesh = lagrange.primitive.generateSphere({ radius: 1 });
    const graph = lagrange.scene.meshToScene(mesh);
    const input = kind === "mesh"
      ? lagrange.serialization.serializeMesh(mesh)
      : lagrange.serialization.serializeScene(graph);
    expect(() => lagrange.serialization.deserializeSimpleScene(input)).toThrow();
    const restored = lagrange.serialization.deserializeSimpleScene(input, {
      allowSceneConversion: true, quiet: true,
    });
    expect(restored.getNumMeshes()).toBe(1);
    expect(restored.getNumInstances(0)).toBe(1);
    const data = lagrange.serialization.serializeSimpleScene(restored);
    if (kind === "mesh") {
      expect(() => lagrange.serialization.deserializeMesh(data)).toThrow();
      const result = lagrange.serialization.deserializeMesh(data, { allowSceneConversion: true, quiet: true });
      expect(result.getNumFacets()).toBe(mesh.getNumFacets());
      result.delete();
    } else {
      expect(() => lagrange.serialization.deserializeScene(data)).toThrow();
      const result = lagrange.serialization.deserializeScene(data, { allowSceneConversion: true, quiet: true });
      expect(result.getNumMeshes()).toBe(1);
      result.delete();
    }
    restored.delete();
    graph.delete();
    mesh.delete();
  });
});
