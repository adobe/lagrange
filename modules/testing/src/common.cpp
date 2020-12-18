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

namespace lagrange {
namespace testing {

// A nice thing about this function is that we don't have to rebuild "everything"
// when we change it.
fs::path get_data_path(const fs::path& relative_path)
{
    if (relative_path.is_absolute()) {
        logger().error("Expected relative path, got absolute path: {}", relative_path);
    }
    REQUIRE(relative_path.is_relative());

    fs::path absolute_path = fs::path(TEST_DATA_DIR) / relative_path;

    if (!fs::exists(absolute_path)) {
        logger().error("{} does not exist", absolute_path.string());
    }
    REQUIRE(fs::exists(absolute_path));
    return absolute_path;
}

} // namespace testing
} // namespace lagrange
