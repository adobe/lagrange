/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>

#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/views.h>

#include <random>

TEST_CASE("scene_extension_value", "[scene]")
{
    using Value = lagrange::scene::Value;
    STATIC_CHECK(Value::variant_index<bool>() == 0);
    STATIC_CHECK(Value::variant_index<int>() == 1);
    STATIC_CHECK(Value::variant_index<double>() == 2);
    STATIC_CHECK(Value::variant_index<std::string>() == 3);
    STATIC_CHECK(Value::variant_index<Value::Buffer>() == 4);
    STATIC_CHECK(Value::variant_index<Value::Array>() == 5);
    STATIC_CHECK(Value::variant_index<Value::Object>() == 6);
    // float is not in the variant
    STATIC_CHECK(Value::variant_index<float>() == 7);
    STATIC_CHECK(std::variant_size_v<Value::variant_type> == 7);

    STATIC_CHECK(Value::is_variant_type<bool>());
    STATIC_CHECK(Value::is_variant_type<int>());
    STATIC_CHECK(Value::is_variant_type<double>());
    STATIC_CHECK(Value::is_variant_type<std::string>());
    STATIC_CHECK(Value::is_variant_type<Value::Buffer>());
    STATIC_CHECK(Value::is_variant_type<Value::Array>());
    STATIC_CHECK(!Value::is_variant_type<std::any>());

    Value bool_value = Value(true);
    REQUIRE(bool_value.is_bool());
    REQUIRE(!bool_value.is_number());
    REQUIRE(bool_value.get<bool>());

    Value bool_value_copy = bool_value;
    REQUIRE(bool_value_copy.is_bool());

    Value int_value = Value(123);
    REQUIRE(!int_value.is_real());
    REQUIRE(int_value.is_int());
    REQUIRE(int_value.is_number());
    REQUIRE(int_value.get<int>() == 123);

    Value int_value_copy = int_value;
    REQUIRE(int_value_copy.is_int());
    REQUIRE(int_value_copy.get_int() == 123);

    Value real_value = Value(123.4);
    REQUIRE(real_value.is_real());
    REQUIRE(real_value.is_number());

    Value string_value = Value("hello");
    REQUIRE(string_value.is_string());
    REQUIRE(string_value.get_string() == "hello");

    Value string_value_copy = string_value;
    REQUIRE(string_value_copy.get_string() == "hello");

    Value buffer_value = Value::create_buffer();
    REQUIRE(buffer_value.is_buffer());
    REQUIRE(buffer_value.size() == 0);

    Value buffer_value_copy = buffer_value;

    Value array_value = Value(Value::Array{bool_value, int_value});
    array_value.get_array().push_back(string_value);
    REQUIRE(array_value.is_array());
    REQUIRE(array_value.size() == 3);
    REQUIRE(array_value[0].get_bool() == true);
    REQUIRE(array_value[1].get_int() == 123);

    Value array_value_copy = array_value;


    Value object_value = Value::create_object();
    REQUIRE(object_value.is_object());
    REQUIRE(object_value.size() == 0);
    object_value.get_object().insert({"array", array_value});
    object_value.get_object().insert({"number", real_value});
    object_value.get_object().insert({"string", string_value});
    REQUIRE(object_value.size() == 3);
    REQUIRE(object_value["array"].is_array());
    REQUIRE(object_value["number"].get_real() == 123.4);
    REQUIRE(object_value["string"].get_string() == "hello");
}

TEST_CASE("Scene: convert", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;

    // Create dummy mesh
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    vertex_ref(mesh).setRandom();
    mesh.add_triangles(10);
    std::mt19937 rng;
    std::uniform_int_distribution<Index> dist(0, 9);
    facet_ref(mesh).unaryExpr([&](auto) { return dist(rng); });

    auto mesh2 = lagrange::scene::scene_to_mesh(lagrange::scene::mesh_to_scene(mesh));
    REQUIRE(vertex_view(mesh) == vertex_view(mesh2));
    REQUIRE(facet_view(mesh) == facet_view(mesh2));
}
