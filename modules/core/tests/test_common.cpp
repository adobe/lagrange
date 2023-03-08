/*
 * Copyright 2017 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/range.h>
#include <Eigen/Core>

TEST_CASE("INVALID", "")
{
    using namespace lagrange;

    using Mesh = TriangleMesh3D;
    using Index = Mesh::Index; // should be 'int'. If this changes this test might require changing.

    REQUIRE(invalid<Index>() == std::numeric_limits<Index>::max());
    REQUIRE(invalid<Index>() == std::numeric_limits<int>::max());
    REQUIRE(
        invalid<Index>() !=
        static_cast<int>(std::numeric_limits<unsigned int>::max())); // different types can have
                                                                     // different INVALID values.
    REQUIRE(std::is_arithmetic<Index>::value);
}

TEST_CASE("MoveData", "[common][eigen]")
{
    using namespace lagrange;
    using MatrixType = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
    MatrixType V1(3, 3);
    MatrixType V2(3, 3);
    V1.setConstant(1);
    V2.setConstant(M_PI);

    void* V1_ptr = V1.data();
    void* V2_ptr = V2.data();
    REQUIRE(V1_ptr != V2_ptr);

    SECTION("std move")
    {
        V2 = std::move(V1);
        REQUIRE(V2.data() == V1_ptr);
    }

    SECTION("eigen move")
    {
        Eigen::PlainObjectBase<MatrixType>& V3 = V1;
        Eigen::PlainObjectBase<MatrixType>& V4 = V2;
        REQUIRE(V1.data() == V1_ptr);
        REQUIRE(V3.data() == V1_ptr);
        REQUIRE(V4.data() == V2_ptr);
        REQUIRE(V2.data() == V2_ptr);

        SECTION("Move PlainObjectBase into PlainObjectBase")
        {
            // Does not compile, operator= is protected.
            // V4 = std::move(V3);
        }

        SECTION("Move PlainObjectBase into Matrix")
        {
            // This result in a copy!
            V4.derived() = std::move(V3);
            V3.resize(0, 3);
            REQUIRE(V4(0, 0) == 1);
            REQUIRE(V2(0, 0) == 1);
            REQUIRE(V4.data() == V2_ptr);
            REQUIRE(V2.data() == V2_ptr);
        }

        SECTION("Move Matrix into Matrix")
        {
            // No copy is done.  So getting type right is important!
            V4.derived() = std::move(V3.derived());
            V3.resize(0, 3);
            REQUIRE(V4(0, 0) == 1);
            REQUIRE(V2(0, 0) == 1);
            REQUIRE(V4.data() == V1_ptr);
            REQUIRE(V2.data() == V1_ptr);
        }
    }

    SECTION("eigen move with a chain of references")
    {
        // V2 <-- V3 <-- V4.
        Eigen::PlainObjectBase<MatrixType>& V3 = V2;
        Eigen::PlainObjectBase<MatrixType>& V4 = V3;
        V4.derived() = std::move(V1);
        V1.resize(0, 3);

        REQUIRE(V4.data() == V1_ptr);
        REQUIRE(V3.data() == V1_ptr);
        REQUIRE(V2.data() == V1_ptr);
    }

    SECTION("lagrange move")
    {
        move_data(V1, V2);
        REQUIRE(V2.data() == V1_ptr);
    }

    SECTION("lagrange move on different Eigen types")
    {
        Eigen::Matrix<float, Eigen::Dynamic, 1> X(3, 1);
        Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> Y(3, 1);

        void* X_ptr = X.data();

        move_data(X, Y);

        // A copy is done here because the Eigen types are not exactly the same.
        REQUIRE(Y.data() != X_ptr);
    }
}
