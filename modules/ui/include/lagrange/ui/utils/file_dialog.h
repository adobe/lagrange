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

namespace lagrange {
namespace ui {

///
/// Opens a native file load dialog.
///
/// @param[in]  extension  Extensions to filter with. E.g. "obj".
///
/// @return     Selected file path.
///
fs::path load_dialog(const std::string& extension);

///
/// Opens a native file save dialog.
///
/// @param[in]  extension  Extensions to filter with. E.g. "obj".
///
/// @return     Selected file path.
///
fs::path save_dialog(const std::string& extension);

} // namespace ui
} // namespace lagrange
