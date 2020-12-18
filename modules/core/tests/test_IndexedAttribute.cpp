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
#include <cmath>

#include <lagrange/IndexedAttributes.h>
#include <lagrange/common.h>

TEST_CASE("IndexedAttribute", "[attribute][indexed]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = int;
    using AttributeArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using IndexArray = Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    IndexedAttributes<AttributeArray, IndexArray> attributes;

    SECTION("Simple use case")
    {
        AttributeArray values(3, 3);
        values.setZero();
        IndexArray indices(3, 3);
        indices.setZero();

        attributes.add_attribute("test", values, indices);
        REQUIRE(attributes.has_attribute("test"));

        const auto& values_ref = attributes.get_attribute_values("test");
        const auto& indices_ref = attributes.get_attribute_indices("test");

        REQUIRE(values == values_ref);
        REQUIRE(indices == indices_ref);

        SECTION("Update check")
        {
            attributes.get_attribute_values("test").setOnes();
            REQUIRE(values_ref.minCoeff() == 1);
        }
    }

    SECTION("Different eigen type and storage order")
    {
        Eigen::Matrix<Scalar, 3, 3> values(3, 3);
        values.setZero();
        Eigen::Matrix<Index, 3, 3> indices(3, 3);
        indices.setZero();

        attributes.add_attribute("test", values, indices);
        REQUIRE(attributes.has_attribute("test"));

        const auto& values_ref = attributes.get_attribute_values("test");
        const auto& indices_ref = attributes.get_attribute_indices("test");

        REQUIRE(values == values_ref);
        REQUIRE(indices == indices_ref);

        SECTION("Update check")
        {
            attributes.get_attribute_values("test").setOnes();
            REQUIRE(values_ref.minCoeff() == 1);
        }
    }
}
