/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/ui/utils/file_dialog.h>

#include <portable-file-dialogs.h>

namespace lagrange {
namespace ui {

namespace {

std::vector<std::string> transform_filters(const std::vector<FileFilter>& filters)
{
    std::vector<std::string> results;
    results.reserve(2 * filters.size());
    for (const auto& filter : filters) {
        results.push_back(filter.name);
        results.push_back(filter.pattern);
    }
    return results;
}

} // namespace

fs::path open_file(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters)
{
    auto selection =
        pfd::open_file(title, default_path.string(), transform_filters(filters)).result();
    if (selection.size() > 2) {
        throw std::runtime_error(
            "Cannot select > 1 file without the multiselect flag. Please report this as a bug");
    }
    if (selection.empty()) {
        return "";
    } else {
        return selection.front();
    }
}

std::vector<fs::path> open_files(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters)
{
    auto selection = pfd::open_file(
                         title,
                         default_path.string(),
                         transform_filters(filters),
                         pfd::opt::multiselect)
                         .result();
    std::vector<fs::path> results;
    results.insert(results.end(), selection.begin(), selection.end());
    return results;
}

fs::path save_file(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters,
    FileSave overwrite_behavior)
{
    pfd::opt option =
        (overwrite_behavior == FileSave::SilentOverwrite ? pfd::opt::force_overwrite
                                                         : pfd::opt::none);
    return pfd::save_file(title, default_path.string(), transform_filters(filters), option)
        .result();
}

fs::path
open_folder(const std::string& title, const fs::path& default_path, FolderOpen open_behavior)
{
    pfd::opt option =
        (open_behavior == FolderOpen::LastOpened ? pfd::opt::force_path : pfd::opt::none);
    return pfd::select_folder(title, default_path.string(), option).result();
}

} // namespace ui
} // namespace lagrange
