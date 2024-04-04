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

#include <lagrange/fs/file_utils.h>

TEST_CASE("file_utils", "[io]")
{
    using namespace lagrange;

    // The executable is not present in WebAssembly's virtual file system, so it does not make sense
    // to call get_executable_path() or get_executable_directory() when building with Emscripten.
#if !__EMSCRIPTEN__
    const auto exec_path = fs::get_executable_path();
    const auto exec_dir = fs::get_executable_directory();

    REQUIRE(!exec_dir.empty());
    REQUIRE(!exec_path.empty());

    CAPTURE(exec_path, exec_dir);
    REQUIRE(exec_path.parent_path() == exec_dir);

    #ifndef TEST_APP_NOPATH
        REQUIRE(exec_path == fs::path(TEST_APP_PATH));
    #endif
#endif

    // The following test should succeed if the unit test is run by ctest, which calls the
    // executable using CMAKE_CURRENT_BINARY_DIR as a working directory by default.
    // Otherwise, calling the unit test executable directly from the command-line will probably
    // fail, unless you are using CMAKE_CURRENT_BINARY_DIR as working directory.
    const auto working_dir = fs::get_current_working_directory();
    // REQUIRE(working_dir == fs::path(TEST_WORK_DIR));

    REQUIRE(fs::get_filename_extension("some_mesh.obj") == ".obj");

    // use fs::path because paths can be different depending on platform.
    fs::path a("path/to/foo.obj");
    fs::path b("path/to");
    REQUIRE(fs::get_base_dir(a.string()) == b.string());

    const std::string data = fs::read_file_to_string(lagrange::testing::get_data_path("open/core/a_simple_text_file.txt"));
    REQUIRE(data.size() == 12);
    REQUIRE(data == "Hello World!");
}
