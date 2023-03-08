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
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/triangle_uv_distortion.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("utils/triangle_uv_distortion", "[core][utils]")
{
    using namespace lagrange;
    using Scalar = double;
    using DM = DistortionMetric;
    constexpr Scalar tol = static_cast<Scalar>(1e-6);

    SECTION("identity mapping")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{0, 1, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{1, 0};
        Scalar v2[2]{0, 1};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(2, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(2, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(4, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(1, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(2, tol));
    }

    SECTION("isotropic scaling")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{0, 1, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{2, 0};
        Scalar v2[2]{0, 2};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(8, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(0.5, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(8.5, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(4, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(2, tol));
    }

    SECTION("Anisotropic scaling")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{0, 1, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{1, 0};
        Scalar v2[2]{0, 2};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(5, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(1.25, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(6.25, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(2, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(2.5, tol));
    }

    SECTION("flipped")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{0, 1, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{1, 0};
        Scalar v2[2]{0, -1};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(2, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(2, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(4, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(-1, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(-2, tol));
    }

    SECTION("degenerate uv triangle")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{0, 1, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{1, 0};
        Scalar v2[2]{2, 0};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(5, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(INFINITY, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(INFINITY, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(0, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(INFINITY, tol));
    }

    SECTION("degenerate 3D triangle")
    {
        Scalar V0[3]{0, 0, 0};
        Scalar V1[3]{1, 0, 0};
        Scalar V2[3]{2, 0, 0};

        Scalar v0[2]{0, 0};
        Scalar v1[2]{1, 0};
        Scalar v2[2]{0, 1};

        Scalar dirichlet = triangle_uv_distortion<DM::Dirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(dirichlet, Catch::Matchers::WithinAbs(INFINITY, tol));

        Scalar inverse_dirichlet =
            triangle_uv_distortion<DM::InverseDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(inverse_dirichlet, Catch::Matchers::WithinAbs(5, tol));

        Scalar symmetric_dirichlet =
            triangle_uv_distortion<DM::SymmetricDirichlet, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(symmetric_dirichlet, Catch::Matchers::WithinAbs(INFINITY, tol));

        Scalar area_ratio = triangle_uv_distortion<DM::AreaRatio, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(area_ratio, Catch::Matchers::WithinAbs(INFINITY, tol));

        Scalar mips = triangle_uv_distortion<DM::MIPS, Scalar>(V0, V1, V2, v0, v1, v2);
        REQUIRE_THAT(mips, Catch::Matchers::WithinAbs(INFINITY, tol));
    }
}
