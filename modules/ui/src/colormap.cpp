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
#include <lagrange/ui/Color.h>
#include <lagrange/ui/colormap.h>

namespace {
#include <colormaps/coolwarm.h>
#include <colormaps/inferno.h>
#include <colormaps/magma.h>
#include <colormaps/plasma.h>
#include <colormaps/turbo.h>
#include <colormaps/viridis.h>
}

namespace lagrange {
namespace ui {

Color colormap_lookup(const int palette[], const size_t N, float t)
{
    t = std::min(std::max(t, 0.0f), 1.f);
    const size_t i0 = std::min(static_cast<size_t>(std::floor(t * N - 1)), N - 1);
    const size_t i1 = std::min(static_cast<size_t>(std::ceil(t * N - 1)), N - 1);
    assert(i0 < N);
    assert(i1 < N);

    const Color c0 = Color::integer_to_color(palette[i0]);
    const Color c1 = Color::integer_to_color(palette[i1]);

    t = t * N - i0;
    return c0 + (c1 - c0) * t;
}

Color colormap_lookup(const float palette[][3], const size_t N, float t)
{
    t = std::min(std::max(t, 0.0f), 1.f);
    const size_t i0 = std::min(static_cast<size_t>(std::floor(t * N - 1)), N - 1);
    const size_t i1 = std::min(static_cast<size_t>(std::ceil(t * N - 1)), N - 1);
    assert(i0 < N);
    assert(i1 < N);

    const Color c0 = Color(palette[i0][0], palette[i0][1], palette[i0][2]);
    const Color c1 = Color(palette[i1][0], palette[i1][1], palette[i1][2]);

    t = t * N - i0;
    return c0 + (c1 - c0) * t;
}

Color colormap_viridis(float t)
{
    constexpr size_t N = sizeof(viridis) / sizeof(viridis[0]);
    return colormap_lookup(viridis, N, t);
}

Color colormap_magma(float t)
{
    constexpr size_t N = sizeof(magma) / sizeof(magma[0]);
    return colormap_lookup(magma, N, t);
}

Color colormap_plasma(float t)
{
    constexpr size_t N = sizeof(plasma) / sizeof(plasma[0]);
    return colormap_lookup(plasma, N, t);
}

Color colormap_inferno(float t)
{
    constexpr size_t N = sizeof(inferno) / sizeof(inferno[0]);
    return colormap_lookup(inferno, N, t);
}

Color colormap_turbo(float t)
{
    constexpr size_t N = sizeof(turbo) / sizeof(turbo[0]);
    return colormap_lookup(turbo, N, t);
}

Color colormap_coolwarm(float t)
{
    constexpr size_t N = sizeof(coolwarm) / sizeof(coolwarm[0]);
    return colormap_lookup(coolwarm, N, t);
}

} // namespace ui
} // namespace lagrange
