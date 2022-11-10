/*
 * Source: https://github.com/ocornut/imgui/issues/1901
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018 Zelimir Fedoran
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * This file has been modified by Adobe.
 *
 * All modifications are Copyright 2021 Adobe.
 */

#pragma once

#include <imgui.h>

namespace lagrange {
namespace ui {
namespace imgui {

bool BufferingBar(
    const char* label,
    float value,
    const ImVec2& size_arg,
    const ImU32& bg_col,
    const ImU32& fg_col);

bool Spinner(const char* label, float radius, int thickness, const ImU32& color);

} // namespace imgui
} // namespace ui
} // namespace lagrange
