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
import type { AttributeDType } from "../src/modules/core.js";

var lagrange: Lagrange;

beforeAll(async () => {
  lagrange = await loadLagrange();
});

/** Helper: create a simple triangle mesh (3 vertices, 1 facet, 3 corners). */
function makeTriangleMesh() {
  const mesh = new lagrange.core.SurfaceMesh(3);
  mesh.addVertex([0, 0, 0]);
  mesh.addVertex([1, 0, 0]);
  mesh.addVertex([0, 1, 0]);
  mesh.addTriangle(0, 1, 2);
  return mesh;
}

describe("attribute: create / read round trip", () => {
  test("per-vertex float32 attribute round-trips", () => {
    const mesh = makeTriangleMesh();
    const id = mesh.createAttribute("weight", {
      element: "vertex",
      usage: "scalar",
      data: new Float32Array([1, 2, 3]),
    });
    expect(typeof id).toBe("number");
    expect(mesh.hasAttribute("weight")).toBe(true);

    const attr = mesh.getAttribute("weight");
    expect(attr.id).toBe(id);
    expect(attr.name).toBe("weight");
    expect(attr.element).toBe("vertex");
    expect(attr.usage).toBe("scalar");
    expect(attr.numChannels).toBe(1);
    expect(attr.numElements).toBe(3);
    expect(attr.dtype).toBe("float32");
    expect(attr.data).toBeInstanceOf(Float32Array);
    expect(Array.from(attr.data)).toEqual([1, 2, 3]);
    mesh.delete();
  });

  test("dtype defaults to float64 for a plain number[]", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("plain", {
      element: "vertex",
      data: [1, 2, 3],
    });
    const attr = mesh.getAttribute("plain");
    expect(attr.dtype).toBe("float64");
    expect(attr.data).toBeInstanceOf(Float64Array);
    expect(Array.from(attr.data)).toEqual([1, 2, 3]);
    mesh.delete();
  });

  test("multi-channel color attribute (uint8, 3 channels)", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("color", {
      element: "vertex",
      usage: "color",
      numChannels: 3,
      data: new Uint8Array([255, 0, 0, 0, 255, 0, 0, 0, 255]),
    });
    const attr = mesh.getAttribute("color");
    expect(attr.numChannels).toBe(3);
    expect(attr.numElements).toBe(3);
    expect(attr.dtype).toBe("uint8");
    expect(attr.data).toBeInstanceOf(Uint8Array);
    expect(Array.from(attr.data)).toEqual([255, 0, 0, 0, 255, 0, 0, 0, 255]);
    mesh.delete();
  });

  test("Uint8ClampedArray data infers dtype uint8", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("clamped", {
      element: "vertex",
      data: new Uint8ClampedArray([1, 2, 3]),
    });
    const attr = mesh.getAttribute("clamped");
    expect(attr.dtype).toBe("uint8");
    expect(Array.from(attr.data)).toEqual([1, 2, 3]);
    mesh.delete();
  });

  test("per-facet attribute with explicit dtype overrides inferred one", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("materialId", {
      element: "facet",
      usage: "scalar",
      dtype: "int32",
      data: [7],
    });
    const attr = mesh.getAttribute("materialId");
    expect(attr.element).toBe("facet");
    expect(attr.numElements).toBe(1);
    expect(attr.dtype).toBe("int32");
    expect(attr.data).toBeInstanceOf(Int32Array);
    expect(Array.from(attr.data)).toEqual([7]);
    mesh.delete();
  });

  test("creating without data auto-sizes to the mesh's current element count", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("empty", { element: "vertex" });
    const attr = mesh.getAttribute("empty");
    expect(attr.numElements).toBe(3);
    expect(attr.numChannels).toBe(1);
    mesh.delete();
  });

  test("value element is sized by the provided data, independent of mesh size", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("freeBuffer", {
      element: "value",
      numChannels: 2,
      data: [1, 2, 3, 4, 5, 6],
    });
    const attr = mesh.getAttribute("freeBuffer");
    expect(attr.element).toBe("value");
    expect(attr.numChannels).toBe(2);
    expect(attr.numElements).toBe(3);
    mesh.delete();
  });

  test.each<AttributeDType>([
    "int8",
    "int16",
    "int32",
    "uint8",
    "uint16",
    "uint32",
    "float32",
    "float64",
  ])("round-trips a %s value-element attribute", (dtype) => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("buf", {
      element: "value",
      dtype,
      data: [0, 1, 5, 10],
    });
    const attr = mesh.getAttribute("buf");
    expect(attr.dtype).toBe(dtype);
    expect(Array.from(attr.data)).toEqual([0, 1, 5, 10]);
    mesh.delete();
  });

  test("int64/uint64 attributes are widened to Float64Array on read", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("bigInts", {
      element: "value",
      dtype: "int64",
      data: [10, -20, Number.MAX_SAFE_INTEGER],
    });
    const attr = mesh.getAttribute("bigInts");
    expect(attr.dtype).toBe("int64");
    expect(attr.data).toBeInstanceOf(Float64Array);
    expect(Array.from(attr.data)).toEqual([10, -20, Number.MAX_SAFE_INTEGER]);

    mesh.createAttribute("bigUints", {
      element: "value",
      dtype: "uint64",
      data: [10, 20, 30],
    });
    const uattr = mesh.getAttribute("bigUints");
    expect(uattr.dtype).toBe("uint64");
    expect(uattr.data).toBeInstanceOf(Float64Array);
    expect(Array.from(uattr.data)).toEqual([10, 20, 30]);
    mesh.delete();
  });

  test("int64 value exactly at 2^53 still round-trips (it is exactly representable)", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("edge", { element: "value", dtype: "int64", data: [2 ** 53] });
    expect(Array.from(mesh.getAttribute("edge").data)).toEqual([2 ** 53]);
    mesh.delete();
  });

  test("rejects a fractional int64/uint64 value", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", { element: "value", dtype: "int64", data: [1.5] }),
    ).toThrow();
    mesh.delete();
  });

  test("rejects int64/uint64 values exactly at the rounded double boundary", () => {
    // INT64_MAX/UINT64_MAX round *up* to 2^63/2^64 as doubles, so those exact
    // values must be rejected even though they equal the naively-converted max.
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad1", { element: "value", dtype: "int64", data: [2 ** 63] }),
    ).toThrow();
    expect(() =>
      mesh.createAttribute("bad2", { element: "value", dtype: "uint64", data: [2 ** 64] }),
    ).toThrow();
    mesh.delete();
  });

  test("rejects BigInt64Array data", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", {
        element: "value",
        data: new BigInt64Array([1n, 2n]) as unknown as number[],
      }),
    ).toThrow();
    mesh.delete();
  });
});

describe("attribute: mutation", () => {
  test("setAttribute overwrites values in place", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", {
      element: "vertex",
      data: new Float32Array([1, 2, 3]),
    });
    mesh.setAttribute("weight", [4, 5, 6]);
    const attr = mesh.getAttribute("weight");
    expect(Array.from(attr.data)).toEqual([4, 5, 6]);
    mesh.delete();
  });

  test("setAttribute throws on length mismatch", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", {
      element: "vertex",
      data: [1, 2, 3],
    });
    expect(() => mesh.setAttribute("weight", [1, 2])).toThrow();
    mesh.delete();
  });

  test("setAttribute on int64/uint64 is atomic: a bad value leaves data unchanged", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("bigInts", { element: "value", dtype: "int64", data: [100, 200, 300] });
    // The bad value (NaN) is in the middle; a non-atomic write would still change index 0.
    expect(() => mesh.setAttribute("bigInts", [999, Number.NaN, 999])).toThrow();
    expect(Array.from(mesh.getAttribute("bigInts").data)).toEqual([100, 200, 300]);
    mesh.delete();
  });

  test("deleteAttribute removes it", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", { element: "vertex", data: [1, 2, 3] });
    expect(mesh.hasAttribute("weight")).toBe(true);
    mesh.deleteAttribute("weight");
    expect(mesh.hasAttribute("weight")).toBe(false);
    expect(() => mesh.getAttribute("weight")).toThrow();
    mesh.delete();
  });

  test("renameAttribute moves data under the new name", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("old", { element: "vertex", data: [1, 2, 3] });
    mesh.renameAttribute("old", "new");
    expect(mesh.hasAttribute("old")).toBe(false);
    expect(mesh.hasAttribute("new")).toBe(true);
    expect(Array.from(mesh.getAttribute("new").data)).toEqual([1, 2, 3]);
    mesh.delete();
  });

  test("duplicateAttribute creates an independent copy", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("src", { element: "vertex", data: [1, 2, 3] });
    const newId = mesh.duplicateAttribute("src", "dst");
    expect(typeof newId).toBe("number");
    expect(mesh.hasAttribute("dst")).toBe(true);

    // Mutating the copy must not affect the original (copy-on-write).
    mesh.setAttribute("dst", [9, 9, 9]);
    expect(Array.from(mesh.getAttribute("src").data)).toEqual([1, 2, 3]);
    expect(Array.from(mesh.getAttribute("dst").data)).toEqual([9, 9, 9]);
    mesh.delete();
  });
});

describe("attribute: id / name lookup", () => {
  test("getAttributeId and getAttributeName round-trip", () => {
    const mesh = makeTriangleMesh();
    const id = mesh.createAttribute("weight", {
      element: "vertex",
      data: [1, 2, 3],
    });
    expect(mesh.getAttributeId("weight")).toBe(id);
    expect(mesh.getAttributeName(id)).toBe("weight");
    mesh.delete();
  });

  test("getAttributeId throws for an unknown name", () => {
    const mesh = makeTriangleMesh();
    expect(() => mesh.getAttributeId("nope")).toThrow();
    mesh.delete();
  });

  test("getAttributeName throws for an invalid id", () => {
    const mesh = makeTriangleMesh();
    expect(() => mesh.getAttributeName(0xffffffff)).toThrow();
    mesh.delete();
  });
});

describe("attribute: indexed attributes", () => {
  function makeIndexedUv(mesh: ReturnType<typeof makeTriangleMesh>) {
    return mesh.createAttribute("uv", {
      element: "indexed",
      usage: "uv",
      numChannels: 2,
      data: new Float32Array([0, 0, 1, 0, 0, 1]), // 3 unique UV pairs
      indices: new Uint32Array([0, 1, 2]), // one per corner
    });
  }

  test("isAttributeIndexed reflects element type", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", { element: "vertex", data: [1, 2, 3] });
    makeIndexedUv(mesh);
    expect(mesh.isAttributeIndexed("weight")).toBe(false);
    expect(mesh.isAttributeIndexed("uv")).toBe(true);
    mesh.delete();
  });

  test("getIndexedAttribute reads values and per-corner indices", () => {
    const mesh = makeTriangleMesh();
    const id = makeIndexedUv(mesh);
    const iattr = mesh.getIndexedAttribute("uv");
    expect(iattr.id).toBe(id);
    expect(iattr.name).toBe("uv");
    expect(iattr.usage).toBe("uv");
    expect(iattr.numChannels).toBe(2);
    expect(iattr.dtype).toBe("float32");
    expect(iattr.values.numElements).toBe(3);
    expect(Array.from(iattr.values.data)).toEqual([0, 0, 1, 0, 0, 1]);
    expect(iattr.indices).toBeInstanceOf(Uint32Array);
    expect(Array.from(iattr.indices)).toEqual([0, 1, 2]);
    mesh.delete();
  });

  test("getAttribute rejects an indexed attribute", () => {
    const mesh = makeTriangleMesh();
    makeIndexedUv(mesh);
    expect(() => mesh.getAttribute("uv")).toThrow();
    mesh.delete();
  });

  test("getIndexedAttribute rejects a non-indexed attribute", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", { element: "vertex", data: [1, 2, 3] });
    expect(() => mesh.getIndexedAttribute("weight")).toThrow();
    mesh.delete();
  });

  test("setAttribute rejects an indexed attribute", () => {
    const mesh = makeTriangleMesh();
    makeIndexedUv(mesh);
    expect(() => mesh.setAttribute("uv", [0, 0, 1, 0, 0, 1])).toThrow();
    mesh.delete();
  });

  test("passing indices for a non-indexed element throws", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", {
        element: "vertex",
        data: [1, 2, 3],
        indices: [0, 1, 2],
      }),
    ).toThrow();
    mesh.delete();
  });
});

describe("attribute: validation errors", () => {
  test("createAttribute requires 'element'", () => {
    const mesh = makeTriangleMesh();
    // @ts-expect-error missing required 'element'
    expect(() => mesh.createAttribute("bad", {})).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects an unknown element", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      // @ts-expect-error intentionally invalid element
      mesh.createAttribute("bad", { element: "nope", data: [1] }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects an unknown usage", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", {
        element: "vertex",
        // @ts-expect-error intentionally invalid usage
        usage: "nope",
        data: [1, 2, 3],
      }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects an unknown dtype", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", {
        element: "value",
        // @ts-expect-error intentionally invalid dtype
        dtype: "nope",
        data: [1, 2, 3],
      }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects a usage/channel-count mismatch", () => {
    const mesh = makeTriangleMesh();
    // "uv" usage requires exactly 2 channels.
    expect(() =>
      mesh.createAttribute("bad", {
        element: "vertex",
        usage: "uv",
        numChannels: 3,
        data: [1, 2, 3, 4, 5, 6, 7, 8, 9],
      }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects vertexIndex usage with a mismatched dtype", () => {
    const mesh = makeTriangleMesh();
    // vertexIndex/facetIndex/cornerIndex/edgeIndex must use the mesh's Index dtype (uint32).
    expect(() =>
      mesh.createAttribute("bad", {
        element: "facet",
        usage: "vertexIndex",
        dtype: "int32",
        data: [0],
      }),
    ).toThrow();
    mesh.createAttribute("good", {
      element: "facet",
      usage: "vertexIndex",
      dtype: "uint32",
      data: [0],
    });
    mesh.delete();
  });

  test("createAttribute rejects a reserved ('$'-prefixed) name by default", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("$reserved", { element: "vertex", data: [1, 2, 3] }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects a duplicate name", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("weight", { element: "vertex", data: [1, 2, 3] });
    expect(() =>
      mesh.createAttribute("weight", { element: "vertex", data: [4, 5, 6] }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects numChannels: 0 on an indexed attribute", () => {
    // Regression test: the core library divides by numChannels while validating an
    // indexed attribute's value buffer, so 0 must be rejected before reaching it.
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", {
        element: "indexed",
        numChannels: 0,
        data: [1, 2, 3],
        indices: [0, 1, 2],
      }),
    ).toThrow();
    // The module must still be usable afterward.
    expect(mesh.getNumVertices()).toBe(3);
    mesh.delete();
  });

  test.each([-1, 1.5, Number.NaN, Number.POSITIVE_INFINITY])(
    "createAttribute rejects an invalid numChannels (%s)",
    (badNumChannels) => {
      const mesh = makeTriangleMesh();
      expect(() =>
        mesh.createAttribute("bad", { element: "vertex", numChannels: badNumChannels, data: [1, 2, 3] }),
      ).toThrow();
      mesh.delete();
    },
  );

  test.each([Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, 1e300])(
    "createAttribute rejects a non-finite/out-of-range int64 value (%s)",
    (bad) => {
      const mesh = makeTriangleMesh();
      expect(() =>
        mesh.createAttribute("bad", { element: "value", dtype: "int64", data: [0, bad] }),
      ).toThrow();
      mesh.delete();
    },
  );

  test("setAttribute rejects a non-finite int64 value", () => {
    const mesh = makeTriangleMesh();
    mesh.createAttribute("bad", { element: "value", dtype: "int64", data: [0, 1, 2] });
    expect(() => mesh.setAttribute("bad", [0, Number.NaN, 2])).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects an index that is out of range for the value count", () => {
    const mesh = makeTriangleMesh();
    // Only 3 UV pairs (indices 0-2) exist; index 3 does not.
    expect(() =>
      mesh.createAttribute("bad", {
        element: "indexed",
        usage: "uv",
        numChannels: 2,
        data: new Float32Array([0, 0, 1, 0, 0, 1]),
        indices: new Uint32Array([0, 1, 3]),
      }),
    ).toThrow();
    mesh.delete();
  });

  test("createAttribute rejects non-empty indices without data", () => {
    const mesh = makeTriangleMesh();
    expect(() =>
      mesh.createAttribute("bad", { element: "indexed", numChannels: 2, indices: [0, 1, 2] }),
    ).toThrow();
    mesh.delete();
  });

  test("a failed createAttribute does not leave the name reserved", () => {
    const mesh = makeTriangleMesh();
    // "uv" requires exactly 2 channels; this fails deep inside the core library.
    expect(() =>
      mesh.createAttribute("uv", { element: "vertex", usage: "uv", numChannels: 3, data: [1, 2, 3, 4, 5, 6, 7, 8, 9] }),
    ).toThrow();
    expect(mesh.hasAttribute("uv")).toBe(false);
    // The name must be reusable afterward.
    mesh.createAttribute("uv", { element: "vertex", usage: "uv", numChannels: 2, data: [0, 0, 1, 0, 0, 1] });
    expect(mesh.hasAttribute("uv")).toBe(true);
    mesh.delete();
  });

  test.each([-1, 1.5, Number.MAX_VALUE])(
    "createAttribute rejects an invalid index value (%s)",
    (badIndex) => {
      const mesh = makeTriangleMesh();
      expect(() =>
        mesh.createAttribute("bad", {
          element: "indexed",
          numChannels: 2,
          data: [0, 0, 1, 0, 0, 1],
          indices: [0, 1, badIndex],
        }),
      ).toThrow();
      mesh.delete();
    },
  );
});
