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

#include <lagrange/ui/api.h>
#include <lagrange/fs/filesystem.h>

#include <vector>

namespace lagrange {
namespace ui {

///
/// RAII Wrapper for fs::path obtained from a file dialog
///
/// If compiled for web under Emscripten:
///     Removes the file from temporary browser filesystem when out of scope
///     If created by `create_output_path`: triggers download of the file when going out of scope
///
class LA_UI_API FileDialogPath
{
public:
    /// Create FileDialogPath from Open File Dialog
    static FileDialogPath make_input_path(const fs::path& path);
    /// Create FileDialogPath from Save File Dialog
    static FileDialogPath make_output_path(const fs::path& path);

    FileDialogPath(FileDialogPath&& other);
    FileDialogPath& operator=(FileDialogPath&& other);
    ~FileDialogPath();

    /// Implicit fs::path conversion
    operator const fs::path &() const { return path(); }
    /// Implicit string conversion.
    operator std::string() const { return string(); }

    /// Convert path to string
    std::string string() const { return path().string(); }

    bool empty() const noexcept { return path().empty(); }

    /// Return underlying path
    const fs::path& path() const;

protected:
    FileDialogPath(const fs::path& path);
    struct FileDialogPathImpl;
    std::unique_ptr<FileDialogPathImpl> m_impl;
};


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
LA_UI_API FileDialogPath open_file(
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
LA_UI_API std::vector<FileDialogPath> open_files(
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
LA_UI_API FileDialogPath save_file(
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
LA_UI_API FileDialogPath open_folder(
    const std::string& title,
    const fs::path& default_path = ".",
    FolderOpen open_behavior = FolderOpen::LastOpened);


namespace utils {
///
///  Transforms a list of FileFilters
///  into the `accept` HTML attribute (https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/accept)
///  If a `*` wildcard is passed as one of the filters, return value will be an empty string.
///  Supports MIME-types and image/*, video/*, and audio/* types
///
/// @param[in]  filters        List of FileFilters
///
/// @return     string for `accept` html attribute
///
/// Example:
/// filters: {{"Label", "*.x *.z *.w image/png"}} output: ".x,.z,.w,image/png"
///
LA_UI_API std::string transform_filters_to_accept(const std::vector<FileFilter>& filters);

} // namespace utils

} // namespace ui
} // namespace lagrange
