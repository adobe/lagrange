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
#include <lagrange/Logger.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

TEST_CASE("Logger", "[next]")
{
    auto console = spdlog::stdout_color_mt("console");
    lagrange::set_logger(console);
    lagrange::logger().info("This is the console logger");
    lagrange::set_logger(nullptr);

    {
        auto scope = spdlog::stdout_color_mt("scope");
        lagrange::ScopedLogLevel _(spdlog::level::debug, *scope);
        lagrange::logger().debug("This is a debug message");
    }
    lagrange::logger().debug("This should not appear");
}
