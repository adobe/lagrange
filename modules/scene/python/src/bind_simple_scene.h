/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/python/tensor_utils.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/utils/assert.h>

#include <Eigen/Core>

#include <type_traits>

namespace lagrange::python {

namespace nb = nanobind;

template <typename Scalar, typename Index>
void bind_simple_scene(nb::module_& m)
{
    using MeshInstance3D = lagrange::scene::MeshInstance<Scalar, Index, 3>;
    nb::class_<MeshInstance3D>(m, "MeshInstance3D", "A single mesh instance in a scene")
        .def(
            nb::init<>(),
            "Creates a new mesh instance with identity transform and mesh_index of 0.")
        .def_rw(
            "mesh_index",
            &MeshInstance3D::mesh_index,
            "Index of the mesh in the scene's mesh array.")
        .def_prop_rw(
            "transform",
            [](MeshInstance3D& self) {
                auto& M = self.transform.matrix();
                using MatrixType = std::decay_t<decltype(M)>;
                static_assert(!MatrixType::IsRowMajor, "Transformation matrix is not column major");

                span<Scalar> data(M.data(), M.size());
                size_t shape[2]{static_cast<size_t>(M.rows()), static_cast<size_t>(M.cols())};
                int64_t stride[2]{1, 4};
                return span_to_tensor(data, shape, stride, nb::cast(&self));
            },
            [](MeshInstance3D& self, Tensor<Scalar> tensor) {
                auto [values, shape, stride] = tensor_to_span(tensor);
                auto& M = self.transform.matrix();
                using MatrixType = std::decay_t<decltype(M)>;
                static_assert(!MatrixType::IsRowMajor, "Transformation matrix is not column major");

                la_runtime_assert(is_dense(shape, stride));
                la_runtime_assert(check_shape(shape, 4, 4));
                if (stride[0] == 1) {
                    // Tensor is col major.
                    std::copy(values.begin(), values.end(), M.data());
                } else {
                    // Tensor is row major.
                    M(0, 0) = values[0];
                    M(0, 1) = values[1];
                    M(0, 2) = values[2];
                    M(0, 3) = values[3];

                    M(1, 0) = values[4];
                    M(1, 1) = values[5];
                    M(1, 2) = values[6];
                    M(1, 3) = values[7];

                    M(2, 0) = values[8];
                    M(2, 1) = values[9];
                    M(2, 2) = values[10];
                    M(2, 3) = values[11];

                    M(3, 0) = values[12];
                    M(3, 1) = values[13];
                    M(3, 2) = values[14];
                    M(3, 3) = values[15];
                }
            },
            R"(4x4 transformation matrix for this instance.

The transformation matrix is stored in column-major order. Both row-major and column-major
input tensors are supported for setting the transform.)");

    using SimpleScene3D = lagrange::scene::SimpleScene<Scalar, Index, 3>;
    nb::class_<SimpleScene3D>(m, "SimpleScene3D", "Simple scene container for instanced meshes")
        .def(nb::init<>(), "Creates an empty scene with no meshes or instances.")
        .def_prop_ro("num_meshes", &SimpleScene3D::get_num_meshes, "Number of meshes in the scene")
        .def(
            "num_instances",
            &SimpleScene3D::get_num_instances,
            "mesh_index"_a,
            R"(Gets the number of instances for a specific mesh.

:param mesh_index: Index of the mesh.

:return: Number of instances of the specified mesh.)")
        .def_prop_ro(
            "total_num_instances",
            &SimpleScene3D::compute_num_instances,
            "Total number of instances for all meshes in the scene")
        .def(
            "get_mesh",
            &SimpleScene3D::get_mesh,
            "mesh_index"_a,
            R"(Gets a copy of the mesh at the specified index.

:param mesh_index: Index of the mesh.

:return: Copy of the mesh.)")
        .def(
            "ref_mesh",
            &SimpleScene3D::ref_mesh,
            "mesh_index"_a,
            R"(Gets a reference to the mesh at the specified index.

:param mesh_index: Index of the mesh.

:return: Reference to the mesh.)")
        .def(
            "get_instance",
            &SimpleScene3D::get_instance,
            "mesh_index"_a,
            "instance_index"_a,
            R"(Gets a specific instance of a mesh.

:param mesh_index: Index of the mesh.
:param instance_index: Index of the instance for that mesh.

:return: The mesh instance.)")
        .def(
            "reserve_meshes",
            &SimpleScene3D::reserve_meshes,
            "num_meshes"_a,
            R"(Reserves storage for meshes.

:param num_meshes: Number of meshes to reserve space for.)")
        .def(
            "add_mesh",
            &SimpleScene3D::add_mesh,
            "mesh"_a,
            R"(Adds a mesh to the scene.

:param mesh: Mesh to add.

:return: Index of the newly added mesh.)")
        .def(
            "reserve_instances",
            &SimpleScene3D::reserve_instances,
            "mesh_index"_a,
            "num_instances"_a,
            R"(Reserves storage for instances of a specific mesh.

:param mesh_index: Index of the mesh.
:param num_instances: Number of instances to reserve space for.)")
        .def(
            "add_instance",
            &SimpleScene3D::add_instance,
            "instance"_a,
            R"(Adds an instance to the scene.

:param instance: Mesh instance to add.

:return: Index of the newly added instance for its mesh.)");

    // Mesh to scene + scene to mesh

    m.def(
        "simple_scene_to_mesh",
        [](const SimpleScene3D& scene,
           bool normalize_normals,
           bool normalize_tangents_bitangents,
           bool preserve_attributes) {
            TransformOptions transform_options;
            transform_options.normalize_normals = normalize_normals;
            transform_options.normalize_tangents_bitangents = normalize_tangents_bitangents;
            return scene::simple_scene_to_mesh(scene, transform_options, preserve_attributes);
        },
        "scene"_a,
        "normalize_normals"_a = TransformOptions{}.normalize_normals,
        "normalize_tangents_bitangents"_a = TransformOptions{}.normalize_tangents_bitangents,
        "preserve_attributes"_a = true,
        R"(Converts a scene into a concatenated mesh with all the transforms applied.

:param scene: Scene to convert.
:param normalize_normals: If enabled, normals are normalized after transformation.
:param normalize_tangents_bitangents: If enabled, tangents and bitangents are normalized after transformation.
:param preserve_attributes: Preserve shared attributes and map them to the output mesh.

:return: Concatenated mesh.)");

    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;
    m.def(
        "mesh_to_simple_scene",
        [](const MeshType& mesh) { return scene::mesh_to_simple_scene<3>(mesh); },
        "mesh"_a,
        R"(Converts a single mesh into a simple scene with a single identity instance of the input mesh.

:param mesh: Input mesh to convert.

:return: Simple scene containing the input mesh.)");

    m.def(
        "meshes_to_simple_scene",
        [](std::vector<MeshType> meshes) {
            return scene::meshes_to_simple_scene<3>(std::move(meshes));
        },
        "meshes"_a,
        R"(Converts a list of meshes into a simple scene with a single identity instance of each input mesh.

:param meshes: Input meshes to convert.

:return: Simple scene containing the input meshes.)");
}

} // namespace lagrange::python
