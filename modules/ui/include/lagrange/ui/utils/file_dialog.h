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
#pragma once

#include <lagrange/fs/filesystem.h>

#include <vector>

namespace lagrange {
namespace ui {

///
/// File filter option.
///
struct FileFilter
{
    /// Name of the filter, e.g. "All files" or "Image files".
    std::string name;

    /// Pattern used for filter, e.g. "*" or *.png *.jpg *.jpeg *.bmp".
    std::string pattern;
};

///
/// Controls the behavior of the file save dialog.
///
enum class FileSave {
    ConfirmOverwrite, ///< Open a confirmation dialog before overwriting a file.
    SilentOverwrite, ///< Force silently overwriting the file.
};

///
/// Controls the behavior of the folder open dialog.
///
enum class FolderOpen {
    LastOpened, ///< On Windows, use the last opened folder as the initial path.
    ForcePath ///< Force folder dialog window to use the provided initial path.
};

///
/// Opens a native file dialog to open a single file.
///
/// @param[in]  title         Dialog title.
/// @param[in]  default_path  Optional default path.
/// @param[in]  filters       Optional list of filters.
///
/// @return     Selected file path.
///
fs::path open_file(
    const std::string& title,
    const fs::path& default_path = ".",
    const std::vector<FileFilter>& filters = {{"All Files", "*"}});

///
/// Opens a native file dialog to open multiple files.
///
/// @param[in]  title         Dialog title.
/// @param[in]  default_path  Optional default path.
/// @param[in]  filters       Optional list of filters.
///
/// @return     Selected file path.
///
std::vector<fs::path> open_files(
    const std::string& title,
    const fs::path& default_path = ".",
    const std::vector<FileFilter>& filters = {{"All Files", "*"}});

///
/// Opens a native file dialog to save a file.
///
/// @param[in]  title               Dialog title.
/// @param[in]  default_path        Optional default path.
/// @param[in]  filters             Optional list of filters.
/// @param[in]  overwrite_behavior  Overwrite behavior.
///
/// @return     Selected file path.
///
fs::path save_file(
    const std::string& title,
    const fs::path& default_path = ".",
    const std::vector<FileFilter>& filters = {{"All Files", "*"}},
    FileSave overwrite_behavior = FileSave::ConfirmOverwrite);

///
/// Opens a native file dialog to select a folder.
///
/// @param[in]  title          Dialog title.
/// @param[in]  default_path   Optional default path.
/// @param[in]  open_behavior  Default path behavior (e.g. use last opened folder by default, or use
///                            the provided path).
///
/// @return     Selected folder path.
///
fs::path open_folder(
    const std::string& title,
    const fs::path& default_path = ".",
    FolderOpen open_behavior = FolderOpen::LastOpened);

} // namespace ui
} // namespace lagrange
