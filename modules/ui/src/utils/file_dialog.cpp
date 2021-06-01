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
#include <nfd.h>

namespace lagrange {
namespace ui {

fs::path load_dialog(const std::string& extension)
{
    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_OpenDialog(extension.c_str(), nullptr, &path);
    if (result != NFD_OKAY) return "";

    auto path_string = fs::path(path);
    free(path);
    return path_string;
}

fs::path save_dialog(const std::string& extension)
{
    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_SaveDialog(extension.c_str(), nullptr, &path);
    if (result == NFD_OKAY) {
        auto path_string = fs::path(path);
        path_string.replace_extension(extension);
        free(path);
        return path_string;
    }
    return "";
}

} // namespace ui
} // namespace lagrange
