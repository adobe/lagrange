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

#include "bind_types.h"
#include "scene_binding_utils.h"
#include "scene_material.h"

#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

using scene::ElementId;

struct NodeUpdate
{
    std::optional<std::string> name;
    std::optional<Eigen::Affine3f> transform;
    std::optional<ElementId> parent;
    std::optional<SafeVector<scene::SceneMeshInstance>> meshes;
};

Eigen::Affine3f read_transform(const val& input)
{
    if (read_scene_array_length(input, "transform") != 16) {
        throw std::invalid_argument("transform must contain 16 column-major coefficients");
    }
    Eigen::Affine3f transform;
    for (unsigned i = 0; i < 16; ++i) {
        transform.matrix().data()[i] = read_scene_float(input[i], "transform");
    }
    const auto& matrix = transform.matrix();
    if (matrix(3, 0) != 0 || matrix(3, 1) != 0 || matrix(3, 2) != 0 || matrix(3, 3) != 1) {
        throw std::invalid_argument("transform must be affine (last row 0, 0, 0, 1)");
    }
    return transform;
}

NodeUpdate read_node_update(const SceneType& scene, const val& opts, ElementId node_id)
{
    NodeUpdate update;
    if (opts.isUndefined()) return update;
    if (opts.isNull() || opts.typeOf().as<std::string>() != "object") {
        throw std::invalid_argument("Node options must be an object");
    }
    if (has_scene_field(opts, "name")) {
        const auto name = opts["name"];
        if (name.typeOf().as<std::string>() != "string") {
            throw std::invalid_argument("Node name must be a string");
        }
        update.name = name.as<std::string>();
    }
    if (has_scene_field(opts, "transform")) update.transform = read_transform(opts["transform"]);
    if (has_scene_field(opts, "parent")) {
        const auto value = opts["parent"];
        update.parent = value.isNull() ? scene::invalid_element
                                       : read_scene_id(value, scene.nodes.size(), "parent node");
    }
    if (has_scene_field(opts, "meshes")) {
        const auto instances = opts["meshes"];
        const unsigned count = read_scene_array_length(instances, "meshes");
        update.meshes.emplace();
        update.meshes->reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            const auto input = instances[i];
            scene::SceneMeshInstance instance;
            instance.mesh = read_scene_id(input["mesh"], scene.meshes.size(), "mesh");
            if (has_scene_field(input, "materials")) {
                const auto materials = input["materials"];
                const unsigned num_materials = read_scene_array_length(materials, "materials");
                instance.materials.reserve(num_materials);
                for (unsigned j = 0; j < num_materials; ++j) {
                    instance.materials.push_back(
                        read_scene_id(materials[j], scene.materials.size(), "material"));
                }
            }
            update.meshes->push_back(std::move(instance));
        }
    }
    if (update.parent) {
        // Validate ancestry after all JS getters finish, in case a getter reparented nodes.
        ElementId ancestor = *update.parent;
        size_t depth = 0;
        while (ancestor != scene::invalid_element) {
            if (ancestor == node_id || depth++ >= scene.nodes.size()) {
                throw std::invalid_argument("node parent would create a cycle");
            }
            ancestor = scene.nodes.at(ancestor).parent;
        }
    }
    return update;
}

void apply_node_update(scene::Node& node, NodeUpdate& update)
{
    if (update.name) node.name = std::move(*update.name);
    if (update.transform) node.transform = *update.transform;
    if (update.meshes) node.meshes = std::move(*update.meshes);
}

void reserve_sibling_slot(SafeVector<ElementId>& siblings)
{
    if (siblings.size() < siblings.capacity()) return;
    const size_t capacity = siblings.capacity();
    const size_t max_size = siblings.max_size();
    if (capacity == max_size) throw std::length_error("Too many scene node references");
    // Allocate before graph mutation, but retain amortized linear append cost.
    siblings.reserve(capacity == 0 ? 1 : capacity > max_size / 2 ? max_size : capacity * 2);
}

ElementId add_node(SceneType& scene, const val& opts)
{
    auto update = read_node_update(scene, opts, scene::invalid_element);
    scene::Node node;
    apply_node_update(node, update);
    node.parent = update.parent.value_or(scene::invalid_element);
    const ElementId parent = node.parent;
    auto& siblings =
        parent == scene::invalid_element ? scene.root_nodes : scene.nodes[parent].children;
    reserve_sibling_slot(siblings);
    const ElementId id = scene.add(std::move(node));
    // Adding a node may reallocate scene.nodes; reacquire the parent's children.
    if (parent == scene::invalid_element) {
        scene.root_nodes.push_back(id);
    } else {
        scene.nodes[parent].children.push_back(id);
    }
    return id;
}

void set_node(SceneType& scene, const val& js_id, const val& opts)
{
    const ElementId id = read_scene_id(js_id, scene.nodes.size(), "node");
    auto update = read_node_update(scene, opts, id);
    auto& node = scene.nodes[id];
    if (update.parent && *update.parent != node.parent) {
        const ElementId parent = *update.parent;
        auto& new_siblings =
            parent == scene::invalid_element ? scene.root_nodes : scene.nodes[parent].children;
        reserve_sibling_slot(new_siblings);
        auto& old_siblings = node.parent == scene::invalid_element
                                 ? scene.root_nodes
                                 : scene.nodes.at(node.parent).children;
        old_siblings.erase(
            std::remove(old_siblings.begin(), old_siblings.end(), id),
            old_siblings.end());
        new_siblings.push_back(id);
        node.parent = parent;
    }
    apply_node_update(node, update);
}

val get_node(const SceneType& scene, const val& js_id)
{
    const auto& node = scene.nodes[read_scene_id(js_id, scene.nodes.size(), "node")];
    val result = val::object();
    result.set("name", node.name);
    result.set("transform", copy_scene_vector(node.transform.matrix()));
    result.set("parent", node.parent == scene::invalid_element ? val::null() : val(node.parent));
    result.set("children", copy_scene_ids(node.children));
    val instances = val::array();
    for (const auto& instance : node.meshes) {
        val value = val::object();
        value.set("mesh", instance.mesh);
        value.set("materials", copy_scene_ids(instance.materials));
        instances.call<void>("push", value);
    }
    result.set("meshes", instances);
    return result;
}

val get_global_transform(const SceneType& scene, const val& js_id)
{
    ElementId id = read_scene_id(js_id, scene.nodes.size(), "node");
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();
    size_t depth = 0;
    while (id != scene::invalid_element) {
        if (depth++ >= scene.nodes.size())
            throw std::invalid_argument("node hierarchy has a cycle");
        const auto& node = scene.nodes.at(id);
        transform = node.transform * transform;
        id = node.parent;
    }
    return copy_scene_vector(transform.matrix());
}

TransformOptions transform_options_from_js(const val& opts)
{
    TransformOptions options;
    apply_opt(opts, "normalizeNormals", options.normalize_normals);
    apply_opt(opts, "normalizeTangentsBitangents", options.normalize_tangents_bitangents);
    apply_opt(opts, "reorient", options.reorient);
    return options;
}

std::vector<MeshType> meshes_from_js(const val& array)
{
    const unsigned length = array["length"].as<unsigned>();
    std::vector<MeshType> meshes;
    meshes.reserve(length);
    for (unsigned i = 0; i < length; ++i) {
        meshes.push_back(array[i].as<MeshType>(emscripten::allow_raw_pointers()));
    }
    return meshes;
}

val meshes_to_js(std::vector<MeshType> meshes)
{
    val array = val::array();
    for (size_t i = 0; i < meshes.size(); ++i) {
        array.set(i, std::move(meshes[i]));
    }
    return array;
}

void check_mesh_index(const SimpleSceneType& scene, Index mesh_index)
{
    if (mesh_index >= scene.get_num_meshes()) {
        throw std::out_of_range("Mesh index out of range");
    }
}

void check_instance_index(const SimpleSceneType& scene, Index mesh_index, Index instance_index)
{
    check_mesh_index(scene, mesh_index);
    if (instance_index >= scene.get_num_instances(mesh_index)) {
        throw std::out_of_range("Instance index out of range");
    }
}

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_scene)
{
    using namespace emscripten;

    class_<SceneType>("Scene")
        .constructor<>()
        .function("getName", optional_override([](const SceneType& s) { return s.name; }))
        .function("setName", optional_override([](SceneType& s, std::string name) {
                      s.name = std::move(name);
                  }))
        .function("getNumMeshes", optional_override([](const SceneType& s) -> size_t {
                      return s.meshes.size();
                  }))
        .function("getMesh", optional_override([](const SceneType& s, const val& id) {
                      return MeshType(s.meshes[read_scene_id(id, s.meshes.size(), "mesh")]);
                  }))
        .function("addMesh", optional_override([](SceneType& s, const MeshType& mesh) {
                      return s.add(mesh);
                  }))
        .function(
            "setMesh",
            optional_override([](SceneType& s, const val& id, const MeshType& mesh) {
                s.meshes[read_scene_id(id, s.meshes.size(), "mesh")] = mesh;
            }))
        .function("getNumNodes", optional_override([](const SceneType& s) -> size_t {
                      return s.nodes.size();
                  }))
        .function("getRootNodes", optional_override([](const SceneType& s) {
                      return copy_scene_ids(s.root_nodes);
                  }))
        .function("getNode", &get_node)
        .function("addNode", optional_override([](SceneType& s) {
                      return add_node(s, val::undefined());
                  }))
        .function("addNode", &add_node)
        .function("setNode", &set_node)
        .function("getGlobalTransform", &get_global_transform)
        .function("getNumMaterials", optional_override([](const SceneType& s) -> size_t {
                      return s.materials.size();
                  }))
        .function("getMaterial", &get_material)
        .function("addMaterial", optional_override([](SceneType& s) {
                      return add_material(s, val::undefined());
                  }))
        .function("addMaterial", &add_material)
        .function("setMaterial", &set_material);

    class_<MeshInstanceType>("MeshInstance")
        .constructor<>()
        .property("meshIndex", &MeshInstanceType::mesh_index)
        .property(
            "transform",
            +[](const MeshInstanceType& instance) -> val {
                const auto& matrix = instance.transform.matrix();
                return val::global("Float64Array")
                    .new_(typed_memory_view(matrix.size(), matrix.data()));
            },
            +[](MeshInstanceType& instance, val data) {
                if (data["length"].as<unsigned>() != 16) {
                    throw std::runtime_error("Transform must contain 16 column-major values");
                }
                MeshInstanceType::AffineTransform transform;
                for (unsigned i = 0; i < 16; ++i) {
                    transform.data()[i] = data[i].as<Scalar>();
                }
                instance.transform = transform;
            });

    class_<SimpleSceneType>("SimpleScene")
        .constructor<>()
        .function(
            "getNumMeshes",
            +[](const SimpleSceneType& scene) { return scene.get_num_meshes(); })
        .function(
            "getNumInstances",
            +[](const SimpleSceneType& scene, Index mesh_index) {
                check_mesh_index(scene, mesh_index);
                return scene.get_num_instances(mesh_index);
            })
        .function(
            "computeNumInstances",
            +[](const SimpleSceneType& scene) { return scene.compute_num_instances(); })
        .function(
            "getMesh",
            +[](const SimpleSceneType& scene, Index mesh_index) -> MeshType {
                check_mesh_index(scene, mesh_index);
                return scene.get_mesh(mesh_index);
            })
        .function(
            "refMesh",
            +[](SimpleSceneType& scene, Index mesh_index) -> MeshType& {
                check_mesh_index(scene, mesh_index);
                return scene.ref_mesh(mesh_index);
            },
            return_value_policy::reference())
        .function(
            "getInstance",
            +[](const SimpleSceneType& scene,
                Index mesh_index,
                Index instance_index) -> MeshInstanceType {
                check_instance_index(scene, mesh_index, instance_index);
                return scene.get_instance(mesh_index, instance_index);
            })
        .function(
            "refInstance",
            +[](SimpleSceneType& scene,
                Index mesh_index,
                Index instance_index) -> MeshInstanceType& {
                check_instance_index(scene, mesh_index, instance_index);
                return scene.ref_instance(mesh_index, instance_index);
            },
            return_value_policy::reference())
        .function(
            "reserveMeshes",
            +[](SimpleSceneType& scene, Index num_meshes) { scene.reserve_meshes(num_meshes); })
        .function(
            "addMesh",
            +[](SimpleSceneType& scene, const MeshType& mesh) {
                if (mesh.get_dimension() != 3) {
                    throw std::runtime_error("SimpleScene requires 3D meshes");
                }
                return scene.add_mesh(mesh);
            })
        .function(
            "reserveInstances",
            +[](SimpleSceneType& scene, Index mesh_index, Index num_instances) {
                check_mesh_index(scene, mesh_index);
                scene.reserve_instances(mesh_index, num_instances);
            })
        .function(
            "addInstance",
            +[](SimpleSceneType& scene, const MeshInstanceType& instance) {
                check_mesh_index(scene, instance.mesh_index);
                return scene.add_instance(instance);
            });

    function("meshToScene", +[](const MeshType& mesh) { return scene::mesh_to_scene(mesh); });
    function(
        "meshesToScene",
        +[](val meshes) { return scene::meshes_to_scene(meshes_from_js(meshes)); });
    function(
        "sceneToMesh",
        +[](const SceneType& scene, val opts) {
            bool preserve_attributes = true;
            apply_opt(opts, "preserveAttributes", preserve_attributes);
            return scene::scene_to_mesh(
                scene,
                transform_options_from_js(opts),
                preserve_attributes);
        });
    function(
        "sceneToMeshes",
        +[](const SceneType& scene, val opts) {
            return meshes_to_js(scene::scene_to_meshes(scene, transform_options_from_js(opts)));
        });
    function(
        "sceneToMeshesAndMaterials",
        +[](const SceneType& scene, val opts) {
            auto result =
                scene::scene_to_meshes_and_materials(scene, transform_options_from_js(opts));
            val material_ids = val::array();
            for (size_t i = 0; i < result.material_ids.size(); ++i) {
                val ids = val::array();
                for (size_t j = 0; j < result.material_ids[i].size(); ++j) {
                    ids.set(j, result.material_ids[i][j]);
                }
                material_ids.set(i, ids);
            }
            val out = val::object();
            out.set("meshes", meshes_to_js(std::move(result.meshes)));
            out.set("materialIds", material_ids);
            return out;
        });

    function(
        "meshToSimpleScene",
        +[](const MeshType& mesh) { return scene::mesh_to_simple_scene(mesh); });
    function(
        "meshesToSimpleScene",
        +[](val meshes) { return scene::meshes_to_simple_scene(meshes_from_js(meshes)); });
    function(
        "simpleSceneToMesh",
        +[](const SimpleSceneType& scene, val opts) {
            check_instance_mesh_indices(scene);
            bool preserve_attributes = true;
            apply_opt(opts, "preserveAttributes", preserve_attributes);
            return scene::simple_scene_to_mesh(
                scene,
                transform_options_from_js(opts),
                preserve_attributes);
        });
    function(
        "simpleSceneToMeshes",
        +[](const SimpleSceneType& scene, val opts) {
            check_instance_mesh_indices(scene);
            return meshes_to_js(
                scene::simple_scene_to_meshes(scene, transform_options_from_js(opts)));
        });
    function(
        "sceneToSimpleScene",
        +[](const SceneType& scene) {
            for (const auto& mesh : scene.meshes) {
                if (mesh.get_dimension() != 3) {
                    throw std::runtime_error("SimpleScene requires 3D meshes");
                }
            }
            return scene::scene_to_simple_scene(scene);
        });
    function(
        "simpleSceneToScene",
        +[](const SimpleSceneType& scene) {
            check_instance_mesh_indices(scene);
            return scene::simple_scene_to_scene(scene);
        });
}
