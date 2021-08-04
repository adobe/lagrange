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

namespace lagrange {

namespace tbb_utils {

///
/// Cancels the execution of the current group of tasks. Cancellation propagates downwards, while
/// exceptions propagates upwards.
///
/// @return     True if the group execution was canceled.
///
bool cancel_group_execution();

///
/// Returns true if the current group execution was canceled.
///
/// @return     True if canceled, False otherwise.
///
bool is_cancelled();

} // namespace tbb_utils

} // namespace lagrange
