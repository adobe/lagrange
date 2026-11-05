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

namespace lagrange::fs {

std::shared_ptr<spdlog::sinks::basic_file_sink_mt> make_file_sink_mt(
    const std::filesystem::path& filename,
    bool truncate)
{
    // TODO: Retire this adapter once we move to spdlog v2.
    return std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        filename.string<spdlog::filename_t::value_type>(),
        truncate);
}

} // namespace lagrange::fs
