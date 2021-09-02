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

#include <lagrange/Logger.h>
#include <lagrange/ui/types/Systems.h>

namespace ui = lagrange::ui;


TEST_CASE("Systems", "[ui][systems]")
{
    ui::Registry r; // dummy registry

    SECTION("add with no id")
    {
        ui::Systems S;
        auto id = S.add(ui::Systems::Stage::Init, [](ui::Registry& /*r*/) {});
        REQUIRE(id != 0);
    }


    SECTION("add with id")
    {
        ui::Systems S;
        auto id = S.add(
            ui::Systems::Stage::Init,
            [](ui::Registry& /*r*/) {},
            ui::string_id("my id"));

        REQUIRE(id == ui::string_id("my id"));
    }


    SECTION("add with existing id")
    {
        ui::Systems S;
        auto id0 = S.add(
            ui::Systems::Stage::Init,
            [](ui::Registry& /*r*/) {},
            ui::string_id("my id"));

        auto id1 = S.add(
            ui::Systems::Stage::Init,
            [](ui::Registry& /*r*/) {},
            ui::string_id("my id"));
        REQUIRE(id0 == ui::string_id("my id"));
        REQUIRE(id1 == 0);
    }


    SECTION("enable disable")
    {
        int runs = 0;
        ui::Systems S;
        auto id = S.add(
            ui::Systems::Stage::Init,
            [&](ui::Registry& /*r*/) { runs++; },
            ui::string_id("my id"));


        REQUIRE(runs == 0);
        S.run(ui::Systems::Stage::Init, r);
        REQUIRE(runs == 1);
        S.enable(id, false);
        S.run(ui::Systems::Stage::Init, r);
        REQUIRE(runs == 1);
    }


    SECTION("enable disable")
    {
        int runs = 0;
        ui::Systems S;
        auto id = S.add(
            ui::Systems::Stage::Init,
            [&](ui::Registry& /*r*/) { runs++; },
            ui::string_id("my id"));


        REQUIRE(runs == 0);
        S.run(ui::Systems::Stage::Init, r);
        REQUIRE(runs == 1);
        S.enable(id, false);
        S.run(ui::Systems::Stage::Init, r);
        REQUIRE(runs == 1);
    }

    SECTION("Execution order outside of group")
    {
        ui::Systems S;
        const int N = int(ui::Systems::Stage::_SIZE);
        std::array<int, N> order;
        int current = 0;

        for (auto i = N - 1; i >= 0; i--) {
            S.add(ui::Systems::Stage(i), [=, &current, &order](ui::Registry& /*r*/) {
                order[current++] = i;
            });
        }

        for (auto i = 0; i < N; i++) {
            S.run(ui::Systems::Stage(i), r);
        }

        REQUIRE(current == N);
        for (auto i = 0; i < N; i++) {
            REQUIRE(order[i] == i);
        }
    }


    SECTION("Execution order within group")
    {
        const auto stage = ui::Systems::Stage::Init;
        int order[3] = {0, 0, 0};
        int current = 0;

        ui::Systems S;
        auto id_a = S.add(stage, [&](ui::Registry& /*r*/) { order[current++] = 1; });
        auto id_b = S.add(stage, [&](ui::Registry& /*r*/) { order[current++] = 2; });
        auto id_c = S.add(stage, [&](ui::Registry& /*r*/) { order[current++] = 3; });

        S.run(ui::Systems::Stage::Init, r);

        // A,B,C
        REQUIRE(current == 3);
        REQUIRE(order[0] == 1);
        REQUIRE(order[1] == 2);
        REQUIRE(order[2] == 3);


        // Reset
        {
            current = 0;
            order[0] = order[1] = order[2] = 0;
        }

        REQUIRE(S.succeeds(id_b, id_c));
        S.run(ui::Systems::Stage::Init, r);
        // A,C,B
        REQUIRE(current == 3);
        REQUIRE(order[0] == 1);
        REQUIRE(order[1] == 3);
        REQUIRE(order[2] == 2);


        // Reset
        {
            current = 0;
            order[0] = order[1] = order[2] = 0;
        }

        REQUIRE(S.succeeds(id_a, id_b));

        S.run(ui::Systems::Stage::Init, r);
        // C,B,A
        REQUIRE(current == 3);
        REQUIRE(order[0] == 3);
        REQUIRE(order[1] == 2);
        REQUIRE(order[2] == 1);
    }
}
