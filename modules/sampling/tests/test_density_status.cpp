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
#include <lagrange/Mesh.h>
#include <lagrange/sampling/DensityStatus.h>
#include <lagrange/sampling/internal/get_density_status.h>

#include <lagrange/testing/common.h>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Density status", "[sampling]")
{
    using AttributeArray = lagrange::TriangleMesh3D::AttributeArray;
    using Scalar = AttributeArray::Scalar;

    SECTION("Non-positive sum")
    {
        AttributeArray density(1, 4);
        density.fill(0);

        const auto status = lagrange::sampling::internal::get_density_status(density);
        REQUIRE(status == lagrange::sampling::DensityStatus::HasNonPositiveSum);
    }

    SECTION("Contains negative")
    {
        AttributeArray density(1, 4);
        density.fill(0);
        density(2) = -1;

        const auto status = lagrange::sampling::internal::get_density_status(density);
        REQUIRE(status == lagrange::sampling::DensityStatus::ContainsNegative);
    }

    SECTION("Contains infinite")
    {
        AttributeArray density(1, 4);
        density.fill(0);
        density(1) = std::numeric_limits<Scalar>::infinity();

        const auto status = lagrange::sampling::internal::get_density_status(density);
        REQUIRE(status == lagrange::sampling::DensityStatus::ContainsNonFiniteOrNan);
    }

    SECTION("Contains nan")
    {
        AttributeArray density(1, 4);
        density.fill(0);
        density(1) = std::numeric_limits<Scalar>::quiet_NaN();

        const auto status = lagrange::sampling::internal::get_density_status(density);
        REQUIRE(status == lagrange::sampling::DensityStatus::ContainsNonFiniteOrNan);
    }

    SECTION("Valid")
    {
        AttributeArray density(1, 4);
        density.fill(0);
        density(1) = 1;
        density(3) = 2;

        const auto status = lagrange::sampling::internal::get_density_status(density);
        REQUIRE(status == lagrange::sampling::DensityStatus::Valid);
    }
}
