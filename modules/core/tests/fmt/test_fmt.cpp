/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "../../include/lagrange/utils/fmt_eigen.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Format Vector", "[fmt]")
{
    Eigen::Vector3f v(1.f, 2.35f, 3.9999f);
    spdlog::info("Vector: {}", v);
}

TEST_CASE("Format Matrix", "[fmt]")
{
    spdlog::set_error_handler([](const std::string& msg) { throw std::runtime_error(msg); });

    Eigen::Matrix3f test;
    // clang-format off
    test << 1.f, 2.75f, 3.19191919f,
           -4.f, 5.17f, 6.f,
            7.f, 8.000000002f, 9.999999999f;
    // clang-format on
#if FMT_VERSION >= 100200
    // This will format without error
    spdlog::info("{:.2f}\n", test);
#elif (FMT_VERSION >= 100000) || (FMT_VERSION >= 90000 && !defined(FMT_DEPRECATED_OSTREAM)) || \
        (defined(LAGRANGE_FMT_EIGEN_FIX) && defined(_MSC_VER))
    // This should also format without error
    spdlog::info("{:.2f}\n", test);
#else
    // This will not compile with legacy ostream formatter.
    REQUIRE_THROWS(spdlog::info("{:.2f}\n", test));
#endif
}

#if FMT_VERSION >= 100200
// This test will only compile with the new nested formatter
TEST_CASE("Format Nested", "[fmt]")
{
    Eigen::Vector3<Eigen::Matrix2f> test;
    // clang-format off
    test.x() << 1.f, 2.75f,
                3.f, 4.999f;
    test.y() << 5.f, 6.00001f,
                -7.f, 8.17f;
    test.z() << 9.f, 10.f,
                11.09f, 12.f;
    // clang-format on
    spdlog::info("{:.2f}\n", test);
}
#endif
