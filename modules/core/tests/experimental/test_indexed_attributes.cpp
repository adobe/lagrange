/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <Eigen/Core>

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/experimental/IndexedAttributeManager.h>

namespace {

template <typename T, int DIM>
using TestArray = Eigen::Matrix<T, Eigen::Dynamic, DIM, Eigen::RowMajor>;

template <typename TargetValueDerived, typename TargetIndexDerived>
void check_indexed_attribute(
    lagrange::experimental::IndexedAttributeManager& manager,
    const std::string& name,
    const Eigen::MatrixBase<TargetValueDerived>& target_values,
    const Eigen::MatrixBase<TargetIndexDerived>& target_indices,
    void* values_ptr = nullptr,
    void* indices_ptr = nullptr)
{
    REQUIRE(manager.has(name));

    using ValueDerived = Eigen::Matrix<
        typename TargetValueDerived::Scalar,
        Eigen::Dynamic,
        Eigen::Dynamic,
        TargetValueDerived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;
    using IndexDerived = Eigen::Matrix<
        typename TargetIndexDerived::Scalar,
        Eigen::Dynamic,
        Eigen::Dynamic,
        TargetIndexDerived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;
    auto values_view = manager.view_values<ValueDerived>(name);
    auto indices_view = manager.view_indices<IndexDerived>(name);

    REQUIRE(values_view == target_values);
    REQUIRE(indices_view == target_indices);

    if (values_ptr != nullptr) {
        // When data pointers differ, data must be copied into the array.
        REQUIRE(values_view.data() == values_ptr);
    }
    if (indices_ptr != nullptr) {
        // When data pointers differ, data must be copied into the array.
        REQUIRE(indices_view.data() == indices_ptr);
    }
}

} // namespace

TEST_CASE("experimental/IndexedAttributes", "[attribute][indexed]")
{
    using namespace lagrange;

    SECTION("Simple usage")
    {
        experimental::IndexedAttributeManager manager;

        TestArray<int, 3> int_array(3, 3);
        int_array.setZero();

        TestArray<float, 3> float_array(3, 3);
        float_array.setOnes();

        manager.add("int_attr");
        manager.set("int_attr", int_array, int_array);

        REQUIRE(manager.has("int_attr"));

        manager.add("float_attr", float_array, int_array);
        REQUIRE(manager.has("float_attr"));

        int_array.setOnes();
        manager.set("int_attr", int_array, int_array);

        REQUIRE(manager.view_values<TestArray<int, 3>>("int_attr").minCoeff() == 1);
        REQUIRE(manager.view_values<TestArray<int, 3>>("int_attr").maxCoeff() == 1);
        REQUIRE(manager.view_indices<TestArray<int, 3>>("int_attr").minCoeff() == 1);
        REQUIRE(manager.view_indices<TestArray<int, 3>>("int_attr").maxCoeff() == 1);

        float_array.setOnes();
        manager.set("float_attr", float_array, int_array);
        REQUIRE(manager.view_values<TestArray<float, 3>>("float_attr").minCoeff() == 1);
        REQUIRE(manager.view_values<TestArray<float, 3>>("float_attr").maxCoeff() == 1);
        REQUIRE(manager.view_indices<TestArray<int, 3>>("float_attr").minCoeff() == 1);
        REQUIRE(manager.view_indices<TestArray<int, 3>>("float_attr").maxCoeff() == 1);

        SECTION("Update")
        {
            auto arr_int = manager.view_values<TestArray<int, 3>>("int_attr");
            auto arr_float = manager.view_values<TestArray<float, 3>>("float_attr");

            arr_int(0, 0) = 10;
            arr_float(0, 0) = 11;

            REQUIRE(manager.view_values<TestArray<int, 3>>("int_attr")(0, 0) == arr_int(0, 0));
            REQUIRE(
                manager.view_values<TestArray<float, 3>>("float_attr")(0, 0) == arr_float(0, 0));
        }
    }

    SECTION("Check for copies")
    {
        experimental::IndexedAttributeManager manager;

        {
            TestArray<short, 3> values(3, 3);
            values.setIdentity();
            TestArray<uint64_t, 3> indices(3, 3);
            indices << 0, 0, 0, 1, 1, 1, 2, 2, 2;
            manager.add("test", values, indices);
        }

        REQUIRE(manager.has("test"));

        // The return type of view_values is an Eigen::Map, so no need
        // to capture it by reference.
        auto values_1 = manager.view_values<TestArray<short, 3>>("test");
        auto values_2 = manager.view_values<TestArray<short, 3>>("test");
        REQUIRE(values_1.data() == values_2.data());

        // To make a copy of the data, fully specify the result type.
        TestArray<short, 3> values_3 = manager.view_values<TestArray<short, 3>>("test");
        REQUIRE(values_1.data() != values_3.data());

        SECTION("What if I remove the attribute?")
        {
            manager.remove("test");
            REQUIRE(!manager.has("test"));
            // TODO:
            // REQUIRE(values_1(0, 0) == 1);
            // ^ This is a bad idea, because it is accessing invalid memory.
            //   Not sure how to prevent this at compile time or check it at run
            //   time.
        }
    }

    SECTION("Creation")
    {
        experimental::IndexedAttributeManager manager;

        SECTION("Create by copying Eigen matrices")
        {
            TestArray<float, 3> values(3, 3);
            values.setOnes();
            TestArray<int, 3> indices(3, 3);
            indices.setZero();

            manager.add("test", values, indices);
            check_indexed_attribute(manager, "test", values, indices);
            manager.remove("test");
        }

        SECTION("Create by moving Eigen matrices")
        {
            TestArray<float, 3> values(3, 3);
            values.setOnes();
            TestArray<int, 3> indices(3, 3);
            indices.setZero();

            void* values_ptr = values.data();
            void* indices_ptr = indices.data();
            auto values_copy = values;
            auto indices_copy = indices;

            manager.add("test", std::move(values), std::move(indices));
            check_indexed_attribute(
                manager,
                "test",
                values_copy,
                indices_copy,
                values_ptr,
                indices_ptr);
            manager.remove("test");
        }

        SECTION("Create by copying Eigen::Map, which will result in copying")
        {
            std::vector<float> values(9, 0);
            std::vector<int> indices(9, 1);

            Eigen::Map<TestArray<float, 3>> values_map(values.data(), 3, 3);
            Eigen::Map<TestArray<int, 3>> indices_map(indices.data(), 3, 3);

            manager.add("test", values_map, indices_map);
            check_indexed_attribute(manager, "test", values_map, indices_map);
            manager.remove("test");
        }

        SECTION("Create by moving Eigen::Map, which will also result in copy")
        {
            std::vector<float> values(9, 0);
            std::vector<int> indices(9, 1);

            Eigen::Map<TestArray<float, 3>> values_map(values.data(), 3, 3);
            Eigen::Map<TestArray<int, 3>> indices_map(indices.data(), 3, 3);

            TestArray<float, 3> values_copy = values_map;
            TestArray<int, 3> indices_copy = indices_map;

            manager.add("test", std::move(values_map), std::move(indices_map));
            check_indexed_attribute(manager, "test", values_copy, indices_copy);
            manager.remove("test");
        }

        SECTION("Create with Array, no copying")
        {
            TestArray<float, 3> values(3, 3);
            values.setOnes();
            TestArray<int, 3> indices(3, 3);
            indices.setZero();

            auto values_array = to_shared_ptr(experimental::create_array(values));
            auto indices_array = to_shared_ptr(experimental::create_array(indices));

            manager.add("test", values_array, indices_array);
            check_indexed_attribute(
                manager,
                "test",
                values,
                indices,
                values_array->data(),
                indices_array->data());
            manager.remove("test");
        }

        SECTION("Create with wrapped Array, no copying")
        {
            TestArray<float, 3> values(3, 3);
            values.setOnes();
            TestArray<int, 3> indices(3, 3);
            indices.setZero();

            auto values_array = to_shared_ptr(experimental::wrap_with_array(values));
            auto indices_array = to_shared_ptr(experimental::wrap_with_array(indices));

            manager.add("test", values_array, indices_array);
            check_indexed_attribute(
                manager,
                "test",
                values,
                indices,
                values.data(),
                indices.data());
            manager.remove("test");
        }

        SECTION("Mixed type creation")
        {
            TestArray<float, 3> values(3, 3);
            values.setOnes();
            TestArray<int, 3> indices(3, 3);
            indices.setZero();

            SECTION("Moved Eigen::Matrix + copied Eigen::Matrix")
            {
                auto values_copy = values;
                void* values_copy_ptr = values_copy.data();

                manager.add("test", std::move(values_copy), indices);
                check_indexed_attribute(manager, "test", values, indices, values_copy_ptr);
                manager.remove("test");
            }

            SECTION("Array + Eigen::Matrix")
            {
                auto values_array = to_shared_ptr(experimental::create_array(values));

                manager.add("test", values_array, indices);
                check_indexed_attribute(manager, "test", values, indices, values_array->data());
                manager.remove("test");
            }

            SECTION("Array + Eigen::Map")
            {
                auto values_array = to_shared_ptr(experimental::create_array(values));
                Eigen::Map<TestArray<int, 3>> indices_map(indices.data(), 3, 3);

                manager.add("test", values_array, indices_map);
                check_indexed_attribute(manager, "test", values, indices, values_array->data());
                manager.remove("test");
            }

            SECTION("Array + moved Eigen::Matrix")
            {
                auto values_array = to_shared_ptr(experimental::create_array(values));
                auto indices_copy = indices;
                void* indices_copy_ptr = indices_copy.data();

                manager.add("test", values_array, std::move(indices_copy));
                check_indexed_attribute(
                    manager,
                    "test",
                    values,
                    indices,
                    values_array->data(),
                    indices_copy_ptr);
                manager.remove("test");
            }
        }
    }

    SECTION("Import and export")
    {
        experimental::IndexedAttributeManager manager;

        TestArray<float, 3> values(3, 3);
        values.setOnes();
        TestArray<int, 3> indices(3, 3);
        indices.setZero();
        void* values_ptr = values.data();
        void* indices_ptr = indices.data();

        manager.add("test", std::move(values), std::move(indices));

        SECTION("Export")
        {
            TestArray<float, 3> tmp_values;
            TestArray<int, 3> tmp_indices;
            manager.export_data("test", tmp_values, tmp_indices);
            REQUIRE(tmp_values.data() == values_ptr);
            REQUIRE(tmp_indices.data() == indices_ptr);
        }

        SECTION("Export with a different Eigen type will result in copy")
        {
            TestArray<float, Eigen::Dynamic> tmp_values(3, 3);
            TestArray<int, Eigen::Dynamic> tmp_indices(3, 3);
            manager.export_data("test", tmp_values, tmp_indices);
            REQUIRE(tmp_values.data() != values_ptr);
            REQUIRE(tmp_indices.data() != indices_ptr);
        }

        SECTION("Exporting raw array will result in a copy")
        {
            std::vector<float> raw_values(9, 0);
            Eigen::Map<TestArray<float, 3>> tmp_values(raw_values.data(), 3, 3);
            TestArray<int, 3> tmp_indices(3, 3);
            tmp_indices.setIdentity();

            values_ptr = tmp_values.data();
            indices_ptr = tmp_indices.data();

            manager.add("test2", tmp_values, std::move(tmp_indices));

            TestArray<float, 3> out_values;
            TestArray<int, 3> out_indices;
            manager.export_data("test2", out_values, out_indices);

            REQUIRE(out_values.data() != values_ptr); // Values are copied.
            REQUIRE(out_indices.data() == indices_ptr); // Indices are not.
        }

        SECTION("Import")
        {
            TestArray<float, 3> tmp_values(3, 3);
            tmp_values.setIdentity();
            TestArray<int, 3> tmp_indices(4, 3);
            tmp_indices.setConstant(10);

            auto values_copy = tmp_values;
            auto indices_copy = tmp_indices;

            values_ptr = tmp_values.data();
            indices_ptr = tmp_indices.data();

            manager.import_data("test", std::move(tmp_values), std::move(tmp_indices));
            check_indexed_attribute(
                manager,
                "test",
                values_copy,
                indices_copy,
                values_ptr,
                indices_ptr);
        }

        SECTION("Import with a different type will fail")
        {
            TestArray<float, Eigen::Dynamic> tmp_values(3, 3);
            tmp_values.setIdentity();
            TestArray<int, Eigen::Dynamic> tmp_indices(4, 3);
            tmp_indices.setConstant(10);

            values_ptr = tmp_values.data();
            indices_ptr = tmp_indices.data();

            // It is not possible to import if the Eigen type does not exactly
            // match (for now)...  A warning should be issued.
            manager.import_data("test", std::move(tmp_values), std::move(tmp_indices));

            auto attr = manager.get("test");
            REQUIRE(attr != nullptr);
            void* out_values_ptr = attr->get_values()->data();
            void* out_indices_ptr = attr->get_indices()->data();

            REQUIRE(values_ptr != out_values_ptr);
            REQUIRE(indices_ptr != out_indices_ptr);
        }

        SECTION("Export -> update -> import")
        {
            manager.export_data("test", values, indices);
            REQUIRE(values.data() == values_ptr);
            REQUIRE(indices.data() == indices_ptr);

            values.setConstant(10);
            indices.setConstant((int)1e5);
            REQUIRE(values.data() == values_ptr);
            REQUIRE(indices.data() == indices_ptr);

            auto values_copy = values;
            auto indices_copy = indices;

            manager.import_data("test", values, indices);
            check_indexed_attribute(
                manager,
                "test",
                values_copy,
                indices_copy,
                values_ptr,
                indices_ptr);
        }
    }
}
