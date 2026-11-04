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

import { afterEach, beforeAll, describe, expect, test } from "vitest";
import { loadLagrange } from "../src/loadLagrange.node.js";
import type { Lagrange, SurfaceMesh, Scene } from "../src/types.js";

type Handle = { delete(): void };
type NodeOptions = Parameters<Scene["setNode"]>[1];

let lagrange: Lagrange;
const owned: Handle[] = [];

beforeAll(async () => {
  lagrange = await loadLagrange();
});

afterEach(() => {
  for (const resource of owned.splice(0).reverse()) resource.delete();
});

function own<T extends Handle>(resource: T): T {
  owned.push(resource);
  return resource;
}

function dispose(resource: Handle) {
  owned.splice(owned.indexOf(resource), 1);
  resource.delete();
}

// All matrices are column-major. Rotation * translation * scale is deliberately
// different from scale * translation * rotation for this off-origin triangle.
const IDENTITY = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
const ROOT_TRANSFORM = [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 10, 20, 30, 1];
const SCALED_TRANSFORM = [2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1];
const SHIFTED_TRANSFORM = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -2, 1, -1, 1];
const SCALED_GLOBAL = [0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 1, 0, 6, 23, 35, 1];
const SHIFTED_GLOBAL = [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 9, 18, 29, 1];
const LOCAL_TRIANGLE = [[1, 0, 0], [2, 0, 0], [1, 1, 0]];
const WORLD_TRIANGLES = [
  [[6, 25, 35], [6, 27, 35], [3, 25, 35]],
  [[9, 19, 29], [9, 20, 29], [8, 19, 29]],
];

function sortPoints(points: number[][]) {
  return points.sort((a, b) => a[0] - b[0] || a[1] - b[1] || a[2] - b[2]);
}

function expectedGeometry(triangles: number[][][]) {
  return {
    vertices: sortPoints(triangles.flat().map((point) => [...point])),
    facets: triangles.map((triangle) => JSON.stringify(sortPoints(triangle.map((point) => [...point])))).sort(),
  };
}

function meshGeometry(mesh: SurfaceMesh) {
  const vertices = Array.from({ length: mesh.getNumVertices() }, (_, id) => Array.from(mesh.getPosition(id)));
  const facets = Array.from({ length: mesh.getNumFacets() }, (_, id) => {
    const points = Array.from(mesh.getFacetVertices(id), (vertex) => [...vertices[vertex]]);
    return JSON.stringify(sortPoints(points));
  }).sort();
  return { vertices: sortPoints(vertices), facets };
}

function makeTriangle(positions = LOCAL_TRIANGLE, weights = [2, 4, 8]) {
  const mesh = own(new lagrange.core.SurfaceMesh(3));
  for (const position of positions) mesh.addVertex(position);
  mesh.addTriangle(0, 1, 2);
  mesh.createAttribute("weight", { element: "vertex", usage: "scalar", data: new Float32Array(weights) });
  return mesh;
}

function makeGraph() {
  const source = makeTriangle();
  const scene = own(new lagrange.scene.Scene());
  scene.setName("shared triangle assembly");
  const mesh = scene.addMesh(source);
  const red = scene.addMaterial({
    name: "paint",
    baseColorValue: new Float32Array([0.75, 0.25, 0.5, 0.75]),
    emissiveValue: [0.125, 0.25, 0.375],
    metallicValue: 0.25,
    roughnessValue: 0.75,
    alphaMode: "mask",
    alphaCutoff: 0.25,
    doubleSided: true,
  });
  const blue = scene.addMaterial({ name: "metal", baseColorValue: [0.25, 0.5, 1, 1], metallicValue: 0.75 });
  const root = scene.addNode({ name: "assembly", transform: ROOT_TRANSFORM });
  const left = scene.addNode({
    name: "scaled",
    parent: root,
    transform: new Float32Array(SCALED_TRANSFORM),
    meshes: [{ mesh, materials: [red] }],
  });
  const right = scene.addNode({
    name: "shifted",
    parent: root,
    transform: SHIFTED_TRANSFORM,
    meshes: [{ mesh, materials: [blue] }],
  });
  return { source, scene, mesh, red, blue, root, left, right };
}

// Capture geometry as well as graph state, so failed setters cannot silently
// wrap an ID to zero or replace a mesh while leaving counts unchanged.
function sceneState(scene: Scene) {
  const meshes = Array.from({ length: scene.getNumMeshes() }, (_, id) => {
    const mesh = scene.getMesh(id);
    try {
      return {
        geometry: meshGeometry(mesh),
        weight: mesh.hasAttribute("weight") ? Array.from(mesh.getAttribute("weight").data) : null,
      };
    } finally {
      mesh.delete();
    }
  });
  return {
    name: scene.getName(),
    roots: scene.getRootNodes(),
    nodes: Array.from({ length: scene.getNumNodes() }, (_, id) => scene.getNode(id)),
    materials: Array.from({ length: scene.getNumMaterials() }, (_, id) => scene.getMaterial(id)),
    meshes,
  };
}

// glTF may renumber nodes and duplicate shared mesh primitives. Compare names,
// relationships, material assignments and real geometry rather than storage IDs.
function namedGraph(scene: Scene) {
  const nodes = Array.from({ length: scene.getNumNodes() }, (_, id) => scene.getNode(id));
  return {
    roots: scene.getRootNodes().map((id) => nodes[id].name).sort(),
    nodes: nodes.map((node, id) => ({
      name: node.name,
      parent: node.parent === null ? null : nodes[node.parent].name,
      children: node.children.map((child) => nodes[child].name).sort(),
      transform: Array.from(node.transform),
      globalTransform: Array.from(scene.getGlobalTransform(id)),
      meshes: node.meshes.map((instance) => {
        const mesh = scene.getMesh(instance.mesh);
        try {
          return {
            geometry: meshGeometry(mesh),
            materials: instance.materials.map((material) => scene.getMaterial(material)),
          };
        } finally {
          mesh.delete();
        }
      }),
    })).sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0),
  };
}

function expectWorldGeometry(scene: Scene, triangles = WORLD_TRIANGLES) {
  const flattened = own(lagrange.scene.sceneToMesh(scene));
  expect(flattened.getNumVertices()).toBe(6);
  expect(flattened.getNumFacets()).toBe(2);
  expect(meshGeometry(flattened)).toEqual(expectedGeometry(triangles));
}

function triangle(): SurfaceMesh {
  const mesh = own(new lagrange.core.SurfaceMesh(3));
  mesh.addVertices([0, 0, 0, 1, 0, 0, 0, 1, 0]);
  mesh.addTriangle(0, 1, 2);
  return mesh;
}

function transform(sx = 1, sy = 1, sz = 1, tx = 0, ty = 0, tz = 0) {
  return new Float64Array([
    sx, 0, 0, 0,
    0, sy, 0, 0,
    0, 0, sz, 0,
    tx, ty, tz, 1,
  ]);
}

function hierarchicalScene() {
  const scene = own(new lagrange.scene.Scene());
  const mesh = scene.addMesh(triangle());
  const material = scene.addMaterial({ name: "material" });
  const root = scene.addNode({ transform: transform(1, 1, 1, 1, 2, 3) });
  scene.addNode({ parent: root, transform: transform(1, 1, 1, 4, 5, 6), meshes: [{ mesh, materials: [material] }] });
  scene.addNode({ parent: root, transform: transform(1, 1, 1, -1, 0, 0), meshes: [{ mesh, materials: [material] }] });
  return scene;
}

describe("Scene graph geometry and ownership", () => {
  test("nodes and materials created without options support editing and mesh instancing", () => {
    const scene = own(new lagrange.scene.Scene());
    const mesh = scene.addMesh(makeTriangle());
    const material = scene.addMaterial();
    scene.setMaterial(material, { name: "paint", baseColorValue: [1, 0, 0, 1] });
    const root = scene.addNode();
    scene.setNode(root, { name: "root", transform: ROOT_TRANSFORM });
    const child = scene.addNode({
      parent: root,
      transform: SCALED_TRANSFORM,
      meshes: [{ mesh, materials: [material] }],
    });
    expect(scene.getRootNodes()).toEqual([root]);
    expect(scene.getNode(root).children).toEqual([child]);
    const flattened = own(lagrange.scene.sceneToMesh(scene));
    expect(meshGeometry(flattened)).toEqual(expectedGeometry([WORLD_TRIANGLES[0]]));
    const ids = Array.from(flattened.getAttribute("material_id").data);
    expect(ids.map((id) => scene.getMaterial(Number(id)).name)).toEqual(["paint"]);
    expect(Array.from(scene.getMaterial(material).baseColorValue)).toEqual([1, 0, 0, 1]);
  });

  test("nested noncommuting transforms place both shared-mesh instances exactly", () => {
    const { scene, mesh, root, left, right, red, blue } = makeGraph();
    expect(scene.getName()).toBe("shared triangle assembly");
    expect(scene.getNumMeshes()).toBe(1);
    expect(scene.getNumNodes()).toBe(3);
    expect(scene.getNumMaterials()).toBe(2);
    expect(scene.getRootNodes()).toEqual([root]);
    expect(scene.getNode(root).children).toEqual([left, right]);
    expect(scene.getNode(left).meshes).toEqual([{ mesh, materials: [red] }]);
    expect(scene.getNode(right).meshes).toEqual([{ mesh, materials: [blue] }]);
    expect(scene.getNode(left).transform).toBeInstanceOf(Float32Array);
    expect(Array.from(scene.getNode(left).transform)).toEqual(SCALED_TRANSFORM);
    expect(scene.getGlobalTransform(left)).toBeInstanceOf(Float32Array);
    expect(Array.from(scene.getGlobalTransform(left))).toEqual(SCALED_GLOBAL);
    expect(Array.from(scene.getGlobalTransform(right))).toEqual(SHIFTED_GLOBAL);
    expect(meshGeometry(own(scene.getMesh(mesh)))).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
    expectWorldGeometry(scene);
  });

  test("addMesh and getMesh own independent COW copies surviving source and scene deletion", () => {
    const source = makeTriangle();
    const scene = own(new lagrange.scene.Scene());
    const id = scene.addMesh(source);
    const first = own(scene.getMesh(id));
    const second = own(scene.getMesh(id));
    source.setAttribute("weight", [100, 200, 300]);
    source.addVertex([7, 8, 9]);
    first.setAttribute("weight", [9, 4, 8]);
    first.addVertex([99, 98, 97]);
    first.clearFacets();
    expect(scene.getNumMeshes()).toBe(1);
    const unchanged = own(scene.getMesh(id));
    expect(meshGeometry(unchanged)).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
    expect(Array.from(unchanged.getAttribute("weight").data)).toEqual([2, 4, 8]);
    expect(Array.from(second.getAttribute("weight").data)).toEqual([2, 4, 8]);
    dispose(source);
    expect(meshGeometry(own(scene.getMesh(id)))).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
    dispose(scene);
    expect(meshGeometry(second)).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
    second.setAttribute("weight", [3, 5, 7]);
    second.addVertex([4, 5, 6]);
    expect(second.getNumVertices()).toBe(4);
    expect(first.getNumVertices()).toBe(4);
    expect(first.getNumFacets()).toBe(0);
    expect(Array.from(first.getAttribute("weight").data).slice(0, 3)).toEqual([9, 4, 8]);
    expect(meshGeometry(unchanged)).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
  });

  test("setMesh replaces shared geometry and attributes for every instance, not prior snapshots", () => {
    const { scene, mesh, left, right } = makeGraph();
    const before = own(scene.getMesh(mesh));
    const replacement = makeTriangle([[1, 0, 2], [2, 0, 2], [1, 1, 2]], [10, 20, 30]);
    replacement.createAttribute("tag", { element: "facet", usage: "scalar", data: new Int32Array([17]) });
    scene.setMesh(mesh, replacement);
    replacement.setAttribute("weight", [100, 200, 300]);
    replacement.clearVertices();
    dispose(replacement);
    expect(scene.getNumMeshes()).toBe(1);
    expect(scene.getNode(left).meshes[0].mesh).toBe(mesh);
    expect(scene.getNode(right).meshes[0].mesh).toBe(mesh);
    const after = own(scene.getMesh(mesh));
    expect(Array.from(after.getAttribute("weight").data)).toEqual([10, 20, 30]);
    expect(Array.from(after.getAttribute("tag").data)).toEqual([17]);
    expect(after.getAttribute("tag").dtype).toBe("int32");
    expect(Array.from(before.getAttribute("weight").data)).toEqual([2, 4, 8]);
    expect(before.hasAttribute("tag")).toBe(false);
    expect(meshGeometry(before)).toEqual(expectedGeometry([LOCAL_TRIANGLE]));
    expectWorldGeometry(scene, [
      [[6, 25, 37], [6, 27, 37], [3, 25, 37]],
      [[9, 19, 31], [9, 20, 31], [8, 19, 31]],
    ]);
  });
});

describe("Scene node editing", () => {
  test("partial patches and nested snapshots do not alias scene data or input arrays", () => {
    const { scene, mesh, red, root, left, right } = makeGraph();
    const empty = scene.addNode();
    expect(scene.getRootNodes()).toEqual([root, empty]);
    const original = sceneState(scene);
    const rootSnapshot = scene.getNode(root);
    const snapshot = scene.getNode(left);
    rootSnapshot.children.length = 0;
    snapshot.name = "not saved";
    snapshot.parent = null;
    snapshot.transform.fill(0);
    snapshot.meshes[0].mesh = 999;
    snapshot.meshes[0].materials.push(999);
    snapshot.meshes.push({ mesh: 999, materials: [] });
    const roots = scene.getRootNodes();
    roots.length = 0;
    const global = scene.getGlobalTransform(left);
    global.fill(0);
    expect(sceneState(scene)).toEqual(original);
    expect(Array.from(scene.getGlobalTransform(left))).toEqual(SCALED_GLOBAL);

    scene.setNode(left, { name: "renamed" });
    expect(scene.getNode(left)).toEqual({ ...original.nodes[left], name: "renamed" });
    const localTransform = new Float64Array(SHIFTED_TRANSFORM);
    const materials = [red];
    const instances = [{ mesh, materials }];
    scene.setNode(left, { transform: localTransform, meshes: instances });
    localTransform.fill(0);
    materials.push(999);
    instances.push({ mesh: 999, materials: [] });
    expect(scene.getNode(left).name).toBe("renamed");
    expect(scene.getNode(left).parent).toBe(root);
    expect(scene.getNode(root).children).toEqual([left, right]);
    expect(scene.getNode(left).meshes).toEqual([{ mesh, materials: [red] }]);
    expect(Array.from(scene.getGlobalTransform(left))).toEqual(SHIFTED_GLOBAL);
    scene.setNode(left, { meshes: [{ mesh }] });
    expect(scene.getNode(left).meshes).toEqual([{ mesh, materials: [] }]);
    scene.setNode(left, { meshes: [] });
    expect(scene.getNode(left).meshes).toEqual([]);
    expect(scene.getNode(right).meshes).toEqual(original.nodes[right].meshes);
  });

  test("reparenting updates both parents and roots once while preserving local transforms", () => {
    const { scene, root, left, right } = makeGraph();
    const other = scene.addNode({ name: "other" });
    const leaf = scene.addNode({ name: "leaf", parent: left });
    scene.setNode(left, { parent: right });
    scene.setNode(left, { parent: right });
    expect(scene.getRootNodes()).toEqual([root, other]);
    expect(scene.getNode(root).children).toEqual([right]);
    expect(scene.getNode(right).children).toEqual([left]);
    expect(scene.getNode(left).children).toEqual([leaf]);
    expect(scene.getNode(leaf).parent).toBe(left);
    expect(Array.from(scene.getNode(left).transform)).toEqual(SCALED_TRANSFORM);
    expect(Array.from(scene.getGlobalTransform(left))).toEqual([0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 1, 0, 5, 21, 34, 1]);
    scene.setNode(left, { parent: null });
    scene.setNode(left, { parent: null });
    expect(scene.getRootNodes()).toEqual([root, other, left]);
    expect(scene.getNode(right).children).toEqual([]);
    expect(scene.getNode(left).parent).toBeNull();
    expect(Array.from(scene.getGlobalTransform(left))).toEqual(SCALED_TRANSFORM);
    scene.setNode(left, { parent: other });
    scene.setNode(left, { name: "still attached" });
    expect(scene.getRootNodes()).toEqual([root, other]);
    expect(scene.getNode(other).children).toEqual([left]);
    expect(scene.getNode(left).parent).toBe(other);
    expect(Array.from(scene.getNode(left).transform)).toEqual(SCALED_TRANSFORM);
  });

  test("self and ancestor cycles reject an entire partial patch", () => {
    const { scene, root, left } = makeGraph();
    const leaf = scene.addNode({ name: "leaf", parent: left });
    const before = sceneState(scene);
    for (const [node, parent] of [[left, left], [root, left], [root, leaf], [left, leaf]]) {
      expect(() => scene.setNode(node, { name: "must not change", parent, meshes: [] })).toThrow();
      expect(sceneState(scene)).toEqual(before);
    }
  });

  test("cycle check re-evaluates ancestry after all options getters finish", () => {
    const scene = own(new lagrange.scene.Scene());
    const a = scene.addNode({ name: "a" });
    const b = scene.addNode({ name: "b" });
    expect(() =>
      scene.setNode(a, {
        name: "must not change",
        parent: b,
        get meshes() {
          scene.setNode(b, { parent: a });
          return [];
        },
      }),
    ).toThrow();
    expect(scene.getNode(a).name).toBe("a");
    expect(scene.getNode(a).parent).toBeNull();
    expect(scene.getNode(b).parent).toBe(a);
    expect(scene.getRootNodes()).toEqual([a]);
    expect(() => scene.getGlobalTransform(b)).not.toThrow();
  });

  test("invalid IDs reject before native indexing without wrapping, coercion or mutation", () => {
    const { scene } = makeGraph();
    const replacement = makeTriangle([[10, 0, 0], [20, 0, 0], [10, 10, 0]]);
    const before = sceneState(scene);
    const invalid = [-1, 0.5, NaN, Infinity, -Infinity, 2 ** 32, 100, "0", null, undefined, true] as unknown as number[];
    for (const id of invalid) {
      const operations = [
        () => own(scene.getMesh(id)),
        () => scene.setMesh(id, replacement),
        () => scene.getNode(id),
        () => scene.setNode(id, { name: "must not change" }),
        () => scene.getGlobalTransform(id),
      ];
      for (const operation of operations) {
        expect(operation).toThrow();
        expect(sceneState(scene)).toEqual(before);
      }
    }
    expect(() => scene.getNode(scene.getNumNodes())).toThrow();
    expect(() => own(scene.getMesh(scene.getNumMeshes()))).toThrow();
    expect(sceneState(scene)).toEqual(before);
  });

  test("bad transforms reject add and partial set atomically", () => {
    const { scene, root, left, mesh, red } = makeGraph();
    const invalidTransforms = [IDENTITY.slice(0, 15), [...IDENTITY, 0]];
    for (const [index, value] of [[0, NaN], [12, Infinity], [3, 1], [7, 1], [11, 1], [15, 2]]) {
      const matrix = [...IDENTITY];
      matrix[index] = value;
      invalidTransforms.push(matrix);
    }
    const before = sceneState(scene);
    for (const matrix of invalidTransforms) {
      expect(() => scene.addNode({ name: "must not add", parent: root, transform: matrix, meshes: [{ mesh, materials: [red] }] })).toThrow();
      expect(sceneState(scene)).toEqual(before);
      expect(() => scene.setNode(left, { name: "must not change", parent: null, meshes: [], transform: matrix })).toThrow();
      expect(sceneState(scene)).toEqual(before);
    }
  });

  test("parent, mesh and every material reference validate before graph mutation", () => {
    const { scene, root, left, mesh, red } = makeGraph();
    const invalid = [-1, 0.5, NaN, Infinity, 2 ** 32, 100, "0", null] as unknown as number[];
    const patches: NodeOptions[] = [];
    for (const id of invalid) {
      if (id !== null) patches.push({ parent: id });
      patches.push({ meshes: [{ mesh, materials: [red] }, { mesh: id }] });
      patches.push({ meshes: [{ mesh, materials: [red, id] }] });
    }
    patches.push({ parent: scene.getNumNodes() });
    patches.push({ meshes: [{ mesh: scene.getNumMeshes() }] });
    patches.push({ meshes: [{ mesh, materials: [scene.getNumMaterials()] }] });
    const before = sceneState(scene);
    for (const patch of patches) {
      expect(() => scene.addNode({ name: "must not add", parent: root, transform: SHIFTED_TRANSFORM, ...patch })).toThrow();
      expect(sceneState(scene)).toEqual(before);
      expect(() => scene.setNode(left, { name: "must not change", parent: null, transform: SHIFTED_TRANSFORM, ...patch })).toThrow();
      expect(sceneState(scene)).toEqual(before);
    }
  });
});

describe("Scene rich round-trips", () => {
  for (const format of ["gltf", "glb"] as const) {
    test(`${format} preserves named hierarchy, material assignment and actual world geometry`, () => {
      const { scene, source } = makeGraph();
      const expected = namedGraph(scene);
      const bytes = lagrange.io.saveSceneToBuffer(scene, format, { quiet: true, exportMaterials: true });
      const restored = own(lagrange.io.loadSceneFromBuffer(bytes, { quiet: true }));
      dispose(scene);
      dispose(source);
      expect(restored.getNumNodes()).toBe(3);
      expect(restored.getNumMaterials()).toBe(2);
      expect(namedGraph(restored)).toEqual(expected);
      expectWorldGeometry(restored);
    });
  }

  test("binary serialization preserves names, shared mesh identity, attributes and hierarchy", () => {
    const { scene, source } = makeGraph();
    const expected = sceneState(scene);
    const expectedNames = namedGraph(scene);
    const bytes = lagrange.serialization.serializeScene(scene, { compress: false });
    const restored = own(lagrange.serialization.deserializeScene(bytes, { quiet: true }));
    dispose(scene);
    dispose(source);
    expect(sceneState(restored)).toEqual(expected);
    expect(namedGraph(restored)).toEqual(expectedNames);
    expect(restored.getNumMeshes()).toBe(1);
    expect(restored.getNode(1).meshes[0].mesh).toBe(restored.getNode(2).meshes[0].mesh);
    expectWorldGeometry(restored);
  });
});

describe("MeshInstance", () => {
  test("defaults and column-major transform assignment", () => {
    const instance = own(new lagrange.scene.MeshInstance());
    expect(instance.meshIndex).toBe(0xffffffff);
    expect(instance.transform).toBeInstanceOf(Float64Array);
    expect(instance.transform).toEqual(transform());
    instance.meshIndex = 7;
    instance.transform = Array.from(transform(2, 3, 4, 5, 6, 7));
    expect(instance.meshIndex).toBe(7);
    expect(instance.transform).toEqual(transform(2, 3, 4, 5, 6, 7));
  });

  test("transform getters and setters copy their data", () => {
    const instance = own(new lagrange.scene.MeshInstance());
    const input = transform(1, 1, 1, 1 / 3);
    instance.transform = input;
    input[12] = 100;
    const output = instance.transform;
    expect(output[12]).toBe(1 / 3);
    output[12] = 200;
    expect(instance.transform[12]).toBe(1 / 3);
    dispose(instance);
    expect(output[12]).toBe(200);
  });

  test("rejects transforms with the wrong length without modifying the instance", () => {
    const instance = own(new lagrange.scene.MeshInstance());
    expect(() => { instance.transform = [1, 2, 3]; }).toThrow();
    expect(instance.transform).toEqual(transform());
  });
});

describe("SimpleScene", () => {
  test("construct, reserve, add meshes and instances", () => {
    const scene = own(new lagrange.scene.SimpleScene());
    expect(scene.getNumMeshes()).toBe(0);
    expect(scene.computeNumInstances()).toBe(0);
    scene.reserveMeshes(2);
    expect(scene.getNumMeshes()).toBe(0);
    const mesh = triangle();
    expect(scene.addMesh(mesh)).toBe(0);
    expect(scene.addMesh(mesh)).toBe(1);
    expect(scene.getNumInstances(0)).toBe(0);
    scene.reserveInstances(0, 2);
    const instance = own(new lagrange.scene.MeshInstance());
    instance.meshIndex = 0;
    expect(scene.addInstance(instance)).toBe(0);
    expect(scene.addInstance(instance)).toBe(1);
    instance.meshIndex = 1;
    expect(scene.addInstance(instance)).toBe(0);
    expect(scene.getNumInstances(0)).toBe(2);
    expect(scene.getNumInstances(1)).toBe(1);
    expect(scene.computeNumInstances()).toBe(3);
    dispose(mesh);
    dispose(instance);
    expect(own(scene.getMesh(0)).getNumFacets()).toBe(1);
  });

  test("getMesh copies while refMesh mutates scene storage", () => {
    const mesh = triangle();
    const scene = own(lagrange.scene.meshToSimpleScene(mesh));
    const copy = own(scene.getMesh(0));
    copy.addVertex([0, 0, 1]);
    expect(scene.refMesh(0).getNumVertices()).toBe(3);
    scene.refMesh(0).addVertex([1, 1, 1]);
    expect(own(scene.getMesh(0)).getNumVertices()).toBe(4);
    expect(mesh.getNumVertices()).toBe(3);
    dispose(scene);
    expect(copy.getNumVertices()).toBe(4);
  });

  test("instances are copied on insertion and retrieval, but refInstance is mutable", () => {
    const scene = own(new lagrange.scene.SimpleScene());
    scene.addMesh(triangle());
    const instance = own(new lagrange.scene.MeshInstance());
    instance.meshIndex = 0;
    instance.transform = transform(1, 1, 1, 2, 3, 4);
    scene.addInstance(instance);
    instance.transform = transform();
    const copy = own(scene.getInstance(0, 0));
    expect(copy.transform).toEqual(transform(1, 1, 1, 2, 3, 4));
    copy.transform = transform();
    expect(scene.refInstance(0, 0).transform[12]).toBe(2);
    scene.refInstance(0, 0).transform = transform(1, 1, 1, 5, 6, 7);
    expect(own(scene.getInstance(0, 0)).transform[12]).toBe(5);
    dispose(scene);
    expect(copy.transform).toEqual(transform());
  });

  test("rejects invalid mesh and instance indices", () => {
    const scene = own(lagrange.scene.meshToSimpleScene(triangle()));
    for (const index of [1, 0xffffffff]) {
      expect(() => scene.getNumInstances(index)).toThrow();
      expect(() => scene.getMesh(index)).toThrow();
      expect(() => scene.refMesh(index)).toThrow();
      expect(() => scene.reserveInstances(index, 1)).toThrow();
      expect(() => scene.getInstance(index, 0)).toThrow();
      expect(() => scene.refInstance(index, 0)).toThrow();
      expect(() => scene.getInstance(0, index)).toThrow();
      expect(() => scene.refInstance(0, index)).toThrow();
    }
    const instance = own(new lagrange.scene.MeshInstance());
    expect(() => scene.addInstance(instance)).toThrow();
    expect(scene.computeNumInstances()).toBe(1);
  });

  test("rejects 2D meshes", () => {
    const mesh = own(new lagrange.core.SurfaceMesh(2));
    const scene = own(new lagrange.scene.SimpleScene());
    expect(() => scene.addMesh(mesh)).toThrow();
    expect(() => lagrange.scene.meshToSimpleScene(mesh)).toThrow();
    expect(() => lagrange.scene.meshesToSimpleScene([mesh])).toThrow();
    const graph = own(lagrange.scene.meshToScene(mesh));
    expect(() => own(lagrange.scene.sceneToSimpleScene(graph))).toThrow();
    expect(graph.getNumMeshes()).toBe(1);
    expect(graph.getNumNodes()).toBe(1);
    expect(mesh.getDimension()).toBe(2);
    expect(scene.getNumMeshes()).toBe(0);
  });
});

describe("scene conversions", () => {
  test.each(["first", "last"])("sceneToSimpleScene rejects mixed dimensions with the 2D mesh %s", (position) => {
    const twoDimensional = own(new lagrange.core.SurfaceMesh(2));
    const threeDimensional = triangle();
    const meshes = position === "first"
      ? [twoDimensional, threeDimensional]
      : [threeDimensional, twoDimensional];
    const scene = own(lagrange.scene.meshesToScene(meshes));
    expect(() => own(lagrange.scene.sceneToSimpleScene(scene))).toThrow();
    expect(scene.getNumMeshes()).toBe(2);
    expect(scene.getNumNodes()).toBe(1);
    expect(twoDimensional.getDimension()).toBe(2);
    expect(threeDimensional.getNumFacets()).toBe(1);
  });

  test("sceneToSimpleScene accepts empty and populated 3D source meshes", () => {
    const empty = own(new lagrange.core.SurfaceMesh(3));
    const scene = own(lagrange.scene.meshesToScene([empty, triangle()]));
    const simple = own(lagrange.scene.sceneToSimpleScene(scene));
    expect(simple.getNumMeshes()).toBe(2);
    expect(simple.getNumInstances(0)).toBe(1);
    expect(simple.getNumInstances(1)).toBe(1);
    const meshes = lagrange.scene.simpleSceneToMeshes(simple).map(own);
    expect(meshes).toHaveLength(2);
    expect(meshes.map((mesh) => mesh.getDimension())).toEqual([3, 3]);
    expect(meshes.map((mesh) => mesh.getNumFacets())).toEqual([0, 1]);
  });

  test("single-mesh conversions preserve geometry and input ownership", () => {
    const mesh = triangle();
    const scene = own(lagrange.scene.meshToScene(mesh));
    const simple = own(lagrange.scene.meshToSimpleScene(mesh));
    expect(scene.getNumMeshes()).toBe(1);
    expect(scene.getNumNodes()).toBe(1);
    expect(simple.getNumMeshes()).toBe(1);
    expect(simple.getNumInstances(0)).toBe(1);
    expect(own(simple.getInstance(0, 0)).transform).toEqual(transform());
    dispose(mesh);
    for (const result of [
      own(lagrange.scene.sceneToMesh(scene)),
      own(lagrange.scene.simpleSceneToMesh(simple)),
    ]) {
      expect(result.getNumVertices()).toBe(3);
      expect(result.getNumFacets()).toBe(1);
      expect(Array.from(result.getPosition(1))).toEqual([1, 0, 0]);
    }
  });

  test("multi-mesh conversions return ordinary arrays of independently owned meshes", () => {
    const mesh = triangle();
    const scene = own(lagrange.scene.meshesToScene([mesh, mesh]));
    const simple = own(lagrange.scene.meshesToSimpleScene([mesh, mesh]));
    expect(scene.getNumMeshes()).toBe(2);
    expect(simple.getNumMeshes()).toBe(2);
    expect(simple.computeNumInstances()).toBe(2);
    const sceneMeshes = lagrange.scene.sceneToMeshes(scene).map(own);
    const simpleMeshes = lagrange.scene.simpleSceneToMeshes(simple).map(own);
    expect(Array.isArray(sceneMeshes)).toBe(true);
    expect(Array.isArray(simpleMeshes)).toBe(true);
    expect(sceneMeshes).toHaveLength(2);
    expect(simpleMeshes).toHaveLength(2);
    expect(own(lagrange.scene.sceneToMesh(scene)).getNumFacets()).toBe(2);
    expect(own(lagrange.scene.simpleSceneToMesh(simple)).getNumFacets()).toBe(2);
    dispose(mesh);
    dispose(scene);
    dispose(simple);
    for (const result of [...sceneMeshes, ...simpleMeshes]) {
      expect(result.getNumFacets()).toBe(1);
      result.addVertex([0, 0, 1]);
      expect(result.getNumVertices()).toBe(4);
    }
  });

  test("empty scenes and mesh lists convert to empty outputs", () => {
    const scene = own(new lagrange.scene.Scene());
    const simple = own(new lagrange.scene.SimpleScene());
    expect(scene.getNumMeshes()).toBe(0);
    expect(scene.getNumNodes()).toBe(0);
    expect(own(lagrange.scene.meshesToScene([])).getNumMeshes()).toBe(0);
    expect(own(lagrange.scene.meshesToSimpleScene([])).getNumMeshes()).toBe(0);
    expect(lagrange.scene.sceneToMeshes(scene)).toEqual([]);
    expect(lagrange.scene.simpleSceneToMeshes(simple)).toEqual([]);
    expect(lagrange.scene.sceneToMeshesAndMaterials(scene)).toEqual({ meshes: [], materialIds: [] });
    expect(own(lagrange.scene.sceneToMesh(scene)).getNumFacets()).toBe(0);
    expect(own(lagrange.scene.simpleSceneToMesh(simple)).getNumFacets()).toBe(0);
    const converted = own(lagrange.scene.sceneToSimpleScene(scene));
    expect(converted.getNumMeshes()).toBe(0);
    expect(converted.computeNumInstances()).toBe(0);
    expect(lagrange.scene.simpleSceneToMeshes(converted)).toEqual([]);
    expect(own(lagrange.scene.simpleSceneToScene(simple)).getNumMeshes()).toBe(0);
  });

  test("meshes without instances survive conversion to Scene and back", () => {
    const simple = own(new lagrange.scene.SimpleScene());
    simple.addMesh(triangle());
    const scene = own(lagrange.scene.simpleSceneToScene(simple));
    const restored = own(lagrange.scene.sceneToSimpleScene(scene));
    expect(restored.getNumMeshes()).toBe(1);
    expect(restored.computeNumInstances()).toBe(0);
    expect(own(restored.getMesh(0)).getDimension()).toBe(3);
    expect(lagrange.scene.simpleSceneToMeshes(restored)).toEqual([]);
  });

  test.each([0, 2, 0xffffffff])("SimpleScene conversions reject changed instance meshIndex %i", (meshIndex) => {
    const mesh = triangle();
    const simple = own(lagrange.scene.meshesToSimpleScene([mesh, mesh]));
    const instance = own(new lagrange.scene.MeshInstance());
    instance.meshIndex = 1;
    simple.addInstance(instance);
    const borrowed = simple.refInstance(1, 1);
    borrowed.meshIndex = meshIndex;

    for (const conversion of ["simpleSceneToMesh", "simpleSceneToMeshes", "simpleSceneToScene"] as const) {
      let failure: unknown;
      try {
        const result = lagrange.scene[conversion](simple);
        if (Array.isArray(result)) result.forEach(own);
        else own(result);
      } catch (error) {
        failure = error;
      }
      expect(failure, conversion).toBeDefined();
      expect(failure, conversion).not.toBeInstanceOf(WebAssembly.RuntimeError);
    }

    borrowed.meshIndex = 1;
    expect(own(lagrange.scene.simpleSceneToMesh(simple)).getNumFacets()).toBe(3);
    expect(lagrange.scene.simpleSceneToMeshes(simple).map(own)).toHaveLength(3);
    expect(own(lagrange.scene.simpleSceneToScene(simple)).getNumNodes()).toBe(4);
  });

  test("Scene to SimpleScene accumulates hierarchical transforms and retains instancing", () => {
    const scene = hierarchicalScene();
    const simple = own(lagrange.scene.sceneToSimpleScene(scene));
    expect(simple.getNumMeshes()).toBe(1);
    expect(simple.getNumInstances(0)).toBe(2);
    expect(own(simple.getInstance(0, 0)).transform).toEqual(transform(1, 1, 1, 5, 7, 9));
    expect(own(simple.getInstance(0, 1)).transform).toEqual(transform(1, 1, 1, 0, 2, 3));
    const meshes = lagrange.scene.simpleSceneToMeshes(simple).map(own);
    expect(Array.from(meshes[0].getPosition(0))).toEqual([5, 7, 9]);
    expect(Array.from(meshes[1].getPosition(0))).toEqual([0, 2, 3]);
    dispose(scene);
    expect(own(simple.getMesh(0)).getNumFacets()).toBe(1);
  });

  test("SimpleScene to Scene round trip preserves transforms and shared source meshes", () => {
    const simple = own(lagrange.scene.meshToSimpleScene(triangle()));
    simple.refInstance(0, 0).transform = transform(2, 3, 4, 5, 6, 7);
    const instance = own(new lagrange.scene.MeshInstance());
    instance.meshIndex = 0;
    instance.transform = transform(1, 1, 1, -2, -3, -4);
    simple.addInstance(instance);
    const scene = own(lagrange.scene.simpleSceneToScene(simple));
    expect(scene.getNumMeshes()).toBe(1);
    expect(scene.getNumNodes()).toBe(3);
    const restored = own(lagrange.scene.sceneToSimpleScene(scene));
    expect(restored.getNumMeshes()).toBe(1);
    expect(restored.computeNumInstances()).toBe(2);
    expect(own(restored.getInstance(0, 0)).transform).toEqual(transform(2, 3, 4, 5, 6, 7));
    expect(own(restored.getInstance(0, 1)).transform).toEqual(instance.transform);
    const flat = own(lagrange.scene.sceneToMesh(scene));
    expect(flat.getNumVertices()).toBe(6);
    expect(Array.from(flat.getPosition(0))).toEqual([5, 6, 7]);
    expect(Array.from(flat.getPosition(1))).toEqual([7, 6, 7]);
    dispose(simple);
    dispose(scene);
    expect(own(restored.getMesh(0)).getNumFacets()).toBe(1);
  });

  test("material conversion returns global material indices and per-facet attributes", () => {
    const scene = hierarchicalScene();
    const result = lagrange.scene.sceneToMeshesAndMaterials(scene);
    result.meshes.forEach(own);
    expect(result.materialIds).toEqual([[0], [0]]);
    expect(result.meshes).toHaveLength(2);
    dispose(scene);
    for (const mesh of result.meshes) {
      expect(Array.from(mesh.getAttribute("material_id").data)).toEqual([0]);
    }
    expect(Array.from(result.meshes[0].getPosition(0))).toEqual([5, 7, 9]);
  });
});

describe("scene conversion options", () => {
  for (const representation of ["Scene", "SimpleScene"] as const) {
    test(`${representation}: preserveAttributes can be disabled`, () => {
      const mesh = triangle();
      mesh.createAttribute("weight", { element: "vertex", data: [1, 2, 3] });
      const simple = own(lagrange.scene.meshToSimpleScene(mesh));
      const scene = own(lagrange.scene.simpleSceneToScene(simple));
      const flatten = (preserveAttributes?: boolean) => own(representation === "Scene"
        ? lagrange.scene.sceneToMesh(scene, { preserveAttributes })
        : lagrange.scene.simpleSceneToMesh(simple, { preserveAttributes }));
      expect(flatten().hasAttribute("weight")).toBe(true);
      expect(flatten(true).hasAttribute("weight")).toBe(true);
      expect(flatten(false).hasAttribute("weight")).toBe(false);
      expect(mesh.hasAttribute("weight")).toBe(true);
    });

    test.each([
      { name: "default normalization", opts: undefined, normal: 1, tangent: 1 },
      {
        name: "disable both normalization options",
        opts: { normalizeNormals: false, normalizeTangentsBitangents: false },
        normal: 12, tangent: 2,
      },
      {
        name: "disable only normal normalization",
        opts: { normalizeNormals: false },
        normal: 12, tangent: 1,
      },
      {
        name: "disable only tangent and bitangent normalization",
        opts: { normalizeTangentsBitangents: false },
        normal: 1, tangent: 2,
      },
    ])(`${representation}: $name`, ({ opts, normal, tangent }) => {
      const mesh = triangle();
      for (const usage of ["normal", "tangent", "bitangent"] as const) {
        mesh.createAttribute(usage, {
          element: "vertex", usage, numChannels: 3, data: [1, 0, 0, 1, 0, 0, 1, 0, 0],
        });
      }
      const simple = own(lagrange.scene.meshToSimpleScene(mesh));
      simple.refInstance(0, 0).transform = transform(2, 3, 4);
      const scene = own(lagrange.scene.simpleSceneToScene(simple));
      const meshes = [
        own(representation === "Scene"
          ? lagrange.scene.sceneToMesh(scene, opts)
          : lagrange.scene.simpleSceneToMesh(simple, opts)),
        ...(representation === "Scene"
          ? lagrange.scene.sceneToMeshes(scene, opts)
          : lagrange.scene.simpleSceneToMeshes(simple, opts)).map(own),
      ];
      if (representation === "Scene") {
        meshes.push(...lagrange.scene.sceneToMeshesAndMaterials(scene, opts).meshes.map(own));
      }
      for (const result of meshes) {
        expect(result.getAttribute("normal").data[0]).toBeCloseTo(normal);
        expect(result.getAttribute("tangent").data[0]).toBeCloseTo(tangent);
        expect(result.getAttribute("bitangent").data[0]).toBeCloseTo(tangent);
      }
    });

    test(`${representation}: reorient controls mirrored facet winding`, () => {
      const simple = own(lagrange.scene.meshToSimpleScene(triangle()));
      simple.refInstance(0, 0).transform = transform(-1, 1, 1);
      const scene = own(lagrange.scene.simpleSceneToScene(simple));
      const original = own(representation === "Scene"
        ? lagrange.scene.sceneToMesh(scene)
        : lagrange.scene.simpleSceneToMesh(simple));
      const reoriented = own(representation === "Scene"
        ? lagrange.scene.sceneToMesh(scene, { reorient: true })
        : lagrange.scene.simpleSceneToMesh(simple, { reorient: true }));
      expect(Array.from(original.getFacetVertices(0))).toEqual([0, 1, 2]);
      expect(Array.from(reoriented.getFacetVertices(0))).toEqual([2, 1, 0]);
    });
  }
});
