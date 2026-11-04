/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/fs/basic_file_sink.h>
#include <lagrange/testing/common.h>

#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

#include <iterator>

TEST_CASE("Basic file sink for filesystem paths", "[fs][logger]")
{
    namespace fs = lagrange::fs;
    fs::path filename("logger.log");
    SECTION("ASCII filename") {}
#if !defined(_WIN32) || defined(SPDLOG_WCHAR_FILENAMES)
    SECTION("Unicode filename")
    {
    #ifdef _WIN32
        filename = fs::path(L"logger_\u65e5\u672c\U0001f600.log");
    #else
        filename = fs::path("logger_\xe6\x97\xa5\xe6\x9c\xac\xf0\x9f\x98\x80.log");
    #endif
    }
#endif

    const auto path = lagrange::testing::get_test_output_path(filename);
    auto write = [](const spdlog::sink_ptr& sink, const char* message) {
        spdlog::logger log("file", sink);
        log.set_pattern("%v");
        log.info("{}", message);
        log.flush();
    };
    auto read = [&]() {
        fs::ifstream file(path);
        REQUIRE(file.good());
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };

    write(fs::make_file_sink_mt(path, true), "first");
    write(fs::make_file_sink_mt(path), "second");
    REQUIRE(read() == "first\nsecond\n");
    write(fs::make_file_sink_mt(path, true), "third");
    REQUIRE(read() == "third\n");
    fs::remove(path);
}
