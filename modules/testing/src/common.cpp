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

#ifdef LA_TESTING_USE_CONFIG
    #include <lagrange/testing/private_config.h>
#endif

#include <lagrange/testing/common.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/io/legacy/load_mesh.impl.h>
#endif

#include <lagrange/Logger.h>

#ifdef EIGEN_USE_MKL_ALL
    #include <mkl.h>
#endif

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

    #include <crtdbg.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif

namespace lagrange {
namespace testing {

#if defined(_WIN32) && defined(_DEBUG)
namespace {

// Terminate on Debug CRT assert/error reports instead of continuing with "Ignore" semantics.
int crt_report_hook(int report_type, char* message, int* /*return_value*/)
{
    if (report_type == _CRT_ERROR || report_type == _CRT_ASSERT) {
        fputs(message, stderr);
        fflush(stderr);
        abort();
    }
    return FALSE;
}

} // namespace
#endif

fs::path get_data_dir()
{
#ifdef TEST_DATA_DIR
    return fs::path(TEST_DATA_DIR);
#else
    static_assert(false, "TEST_DATA_DIR must be defined");
#endif
}

fs::path get_test_output_dir()
{
#ifdef TEST_OUTPUT_DIR
    return fs::path(TEST_OUTPUT_DIR);
#else
    static_assert(false, "TEST_OUTPUT_DIR must be defined");
#endif
}

namespace {

fs::path get_data_path_impl(const fs::path& relative_path)
{
    if (relative_path.is_absolute()) {
        logger().error("Expected relative path, got absolute path: {}", relative_path.string());
    }
    REQUIRE(relative_path.is_relative());

    fs::path absolute_path = get_data_dir() / relative_path;

    if (!fs::exists(absolute_path)) {
        logger().error("{} does not exist", absolute_path.string());
    }
    REQUIRE(fs::exists(absolute_path));
    return absolute_path;
}

} // namespace

fs::path get_data_path(const fs::path& relative_path)
{
    auto result = get_data_path_impl(relative_path);
    REQUIRE(fs::is_regular_file(result));
    return result;
}

fs::path get_data_folder(const fs::path& relative_path)
{
    auto result = get_data_path_impl(relative_path);
    REQUIRE(fs::is_directory(result));
    return result;
}

///
/// Gets a path for writing test output files. Creates the directory if it doesn't exist.
/// The path will be relative to the test output directory (typically build/tmp).
///
/// @param[in]  relative_path  Relative path within the test output directory.
///
/// @return     Absolute path for test output.
///
fs::path get_test_output_path(const fs::path& relative_path)
{
    if (relative_path.is_absolute()) {
        logger().error("Expected relative path, got absolute path: {}", relative_path.string());
    }
    REQUIRE(relative_path.is_relative());

    fs::path base_dir = get_test_output_dir();
    fs::path absolute_path = base_dir / relative_path;

    // Ensure the directory exists (create parent directories if needed)
    fs::path parent_dir = absolute_path.parent_path();
    if (!parent_dir.empty() && !fs::exists(parent_dir)) {
        fs::create_directories(parent_dir);
    }

    return absolute_path;
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
template std::unique_ptr<TriangleMesh3D> load_mesh(const fs::path&);
template std::unique_ptr<QuadMesh3D> load_mesh(const fs::path&);
#endif

void setup_mkl_reproducibility()
{
#ifdef EIGEN_USE_MKL_ALL
    class MySingleton
    {
    public:
        static MySingleton& instance()
        {
            static MySingleton instance;
            return instance;
        }

    private:
        MySingleton()
        {
            // https://software.intel.com/content/www/us/en/develop/articles/introduction-to-the-conditional-numerical-reproducibility-cnr.html
            // https://software.intel.com/content/www/us/en/develop/documentation/onemkl-macos-developer-guide/top/obtaining-numerically-reproducible-results/getting-started-with-conditional-numerical-reproducibility.html
            auto cbwr_branch = mkl_cbwr_get_auto_branch();

            // For some reason anything lower than AVX returns an error on macOS. The different
            // options are:
            // - MKL_CBWR_COMPATIBLE
            // - MKL_CBWR_SSE2
            // - MKL_CBWR_SSSE3
            // - MKL_CBWR_SSE4_1
            // - MKL_CBWR_SSE4_2
            // - MKL_CBWR_AVX
            // - MKL_CBWR_AVX2

    #if __APPLE__
            auto res = mkl_cbwr_set(MKL_CBWR_AVX | MKL_CBWR_STRICT);
    #else
            auto res = mkl_cbwr_set(MKL_CBWR_COMPATIBLE | MKL_CBWR_STRICT);
    #endif
            lagrange::logger().debug("MKL auto cbwr branch: {}", cbwr_branch);
            lagrange::logger().info("Setting MKL reproducibility flag: {}", res);
            la_runtime_assert(res == MKL_CBWR_SUCCESS);
        }
    };
    MySingleton::instance();
#endif
}

void disable_windows_error_dialogs()
{
#ifdef _WIN32
    // Keep interactive dialogs and debug breaks when running under a debugger.
    if (IsDebuggerPresent()) {
        return;
    }

    // Route assert() failure messages to stderr instead of a message box.
    _set_error_mode(_OUT_TO_STDERR);

    // Write the abort() message to stderr, but don't invoke Windows Error Reporting.
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    // Suppress OS-level error popups for hard crashes (e.g. access violations).
    SetErrorMode(
        GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    #ifdef _DEBUG
    // Route Debug CRT reports to stderr instead of the "Debug Assertion Failed" dialog.
    for (int report_type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(report_type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report_type, _CRTDBG_FILE_STDERR);
    }

    // Ensure assert/error reports terminate the process instead of merely logging.
    _CrtSetReportHook(crt_report_hook);
    #endif
#endif
}

} // namespace testing
} // namespace lagrange
