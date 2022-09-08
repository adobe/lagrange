/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/utils/safe_cast.h>

#include <catch2/catch_approx.hpp>

#include <cmath>
#include <limits>

TEST_CASE("SafeCast", "[safe_cast]")
{
    int x0 = lagrange::safe_cast<int>(1.0);
    REQUIRE(x0 == 1);
    int x1 = lagrange::safe_cast<int>(-1.0);
    REQUIRE(x1 == -1);
    LA_REQUIRE_THROWS(lagrange::safe_cast<size_t>(-1.0));
    LA_REQUIRE_THROWS(lagrange::safe_cast<size_t>(1.5));
    float x2 = lagrange::safe_cast<float>(1.0 / 3.0);
    REQUIRE(Catch::Approx(x2 * 3) == 1.0);
    short x3 = lagrange::safe_cast<short>(1);
    REQUIRE(x3 == 1);
    constexpr short max_short = std::numeric_limits<short>::max();
    short x4 = lagrange::safe_cast<short>(max_short);
    REQUIRE(x4 > 0);
    LA_REQUIRE_THROWS(lagrange::safe_cast<short>(max_short + 1));

    const double x5 = 1e2 - std::sqrt(2.0);
    REQUIRE_NOTHROW(lagrange::safe_cast<float>(x5));

    const double x6 = std::sqrt(7) * 1e6 - std::sqrt(2.0);
    REQUIRE_NOTHROW(lagrange::safe_cast<float>(x6));

    const long double x7 = std::sqrt(7) * 1e-40;
    float x8 = lagrange::safe_cast<float>(x7);
    REQUIRE(x8 >= 0.0f);
}

TEST_CASE("SafeCastStressTest", "[safe_cast]" LA_SLOW_FLAG)
{
    auto stress_test = [](auto offset) {
        using Scalar = decltype(offset);
        Scalar base = std::numeric_limits<Scalar>::max();
        constexpr float f_max = std::numeric_limits<float>::max();
        constexpr double d_max = std::numeric_limits<double>::max();
        constexpr long double ld_max = std::numeric_limits<long double>::max();

        while (base > 1.0f) {
            const Scalar value = base - offset;
            if (value <= f_max) {
                REQUIRE_NOTHROW(lagrange::safe_cast<float>(value));
                REQUIRE_NOTHROW(lagrange::safe_cast<float>(-value));
            } else {
                LA_REQUIRE_THROWS(lagrange::safe_cast<float>(value));
                LA_REQUIRE_THROWS(lagrange::safe_cast<float>(-value));
            }

            if (value <= d_max) {
                REQUIRE_NOTHROW(lagrange::safe_cast<double>(value));
                REQUIRE_NOTHROW(lagrange::safe_cast<double>(-value));
            } else {
                LA_REQUIRE_THROWS(lagrange::safe_cast<double>(value));
                LA_REQUIRE_THROWS(lagrange::safe_cast<double>(-value));
            }

            if (value <= ld_max) {
                REQUIRE_NOTHROW(lagrange::safe_cast<long double>(value));
                REQUIRE_NOTHROW(lagrange::safe_cast<long double>(-value));
            } else {
                LA_REQUIRE_THROWS(lagrange::safe_cast<long double>(value));
                LA_REQUIRE_THROWS(lagrange::safe_cast<long double>(-value));
            }
            base /= 2.0f;
        }
    };

    stress_test(sqrt(float(M_PI)));
    stress_test(sqrt(double(M_PI)));
    stress_test(sqrt((long double)(M_PI)));
}


TEST_CASE("SafeEnumCast", "[safe_cast][enum]")
{
    using lagrange::safe_cast_enum;
    enum AnimalFr { cheval = 0, ane, singe, chien };
    enum class AnimalEn : std::uint32_t { horse, donkey, monkey, dog };
    enum class AnimalFa : std::int32_t { asb, khar, meimoon, sag };

    // This should not compile
    // safe_cast_enum<AnimalFa>(AnimalEn::horse);

    // Note that these are valid because an enum variable can hold values
    // not represented by keywords
    // safe_cast_enum<AnimalEn>(-3);
    // safe_cast<AnimalEn>(-100.f);
    // These should fail however.

    // This should fail
    LA_CHECK_THROWS(safe_cast_enum<AnimalEn>(-100.2356f));
    LA_CHECK_THROWS(safe_cast_enum<AnimalEn>(100.2356f));
    LA_CHECK_THROWS(safe_cast_enum<AnimalFr>(-100.2356f));
    LA_CHECK_THROWS(safe_cast_enum<AnimalFr>(100.2356f));
    LA_CHECK_THROWS(safe_cast_enum<AnimalFa>(-100.2356f));
    LA_CHECK_THROWS(safe_cast_enum<AnimalFa>(100.2356f));

    CHECK(safe_cast_enum<AnimalFr>(1) == AnimalFr::ane);
    CHECK(safe_cast_enum<AnimalFr>(1.) == AnimalFr::ane);
    CHECK(safe_cast_enum<AnimalFr>(1.f) == AnimalFr::ane);
    CHECK(safe_cast_enum<AnimalFr>(2) == AnimalFr::singe);
    CHECK(safe_cast_enum<AnimalFr>(2.) == AnimalFr::singe);
    CHECK(safe_cast_enum<AnimalFr>(2.f) == AnimalFr::singe);

    CHECK_NOTHROW(safe_cast_enum<double>(AnimalFr::cheval));
    CHECK_NOTHROW(safe_cast_enum<float>(AnimalFr::cheval));
    CHECK_NOTHROW(safe_cast_enum<int>(AnimalFr::cheval));
    CHECK_NOTHROW(safe_cast_enum<std::uint8_t>(AnimalFr::cheval));
    CHECK_NOTHROW(safe_cast_enum<std::int8_t>(AnimalFr::cheval));

    CHECK_NOTHROW(safe_cast_enum<double>(AnimalEn::horse));
    CHECK_NOTHROW(safe_cast_enum<float>(AnimalEn::horse));
    CHECK_NOTHROW(safe_cast_enum<int>(AnimalEn::horse));
    CHECK_NOTHROW(safe_cast_enum<std::uint8_t>(AnimalEn::horse));
    CHECK_NOTHROW(safe_cast_enum<std::int8_t>(AnimalEn::horse));

    CHECK_NOTHROW(safe_cast_enum<double>(AnimalFa::asb));
    CHECK_NOTHROW(safe_cast_enum<float>(AnimalFa::asb));
    CHECK_NOTHROW(safe_cast_enum<int>(AnimalFa::asb));
    CHECK_NOTHROW(safe_cast_enum<std::uint8_t>(AnimalFa::asb));
    CHECK_NOTHROW(safe_cast_enum<std::int8_t>(AnimalFa::asb));
}
