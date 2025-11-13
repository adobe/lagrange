/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/internal/constants.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <random>

namespace lagrange {
namespace ui {

class Color : public Eigen::Vector4f
{
public:
    using BaseType = Eigen::Vector4f;

    static Color empty() { return Color(0, 0, 0, 0); }
    static Color zero() { return Color(0, 0, 0, 0); }
    static Color black() { return Color(0, 0, 0); }
    static Color white() { return Color(1, 1, 1); }

    static Color red() { return Color(1, 0, 0); }
    static Color green() { return Color(0, 1, 0); }
    static Color blue() { return Color(0, 0, 1); }

    static Color cyan() { return Color(0, 1, 1); }
    static Color yellow() { return Color(1, 1, 0); }
    static Color purple() { return Color(1, 0, 1); }

    Color(const Eigen::Vector4f& color)
        : Eigen::Vector4f(color)
    {}

    Color(const Eigen::Vector3f& rgb, float alpha = 1.0f)
        : Eigen::Vector4f(rgb.x(), rgb.y(), rgb.z(), alpha)
    {}

    Color(const float r, const float g, const float b, const float a)
        : Color(Eigen::Vector4f(r, g, b, a))
    {}

    Color(const float r, const float g, const float b)
        : Color(r, g, b, 1.0f)
    {}

    Color(const float v)
        : Color(v, v, v, 1.0f)
    {}

    Color()
        : Color(0, 0, 0, 0)
    {}

    template <typename Derived>
    Color(const Eigen::MatrixBase<Derived>& p)
        : Eigen::Vector4f(p)
    {}

    template <typename Derived>
    Color& operator=(const Eigen::MatrixBase<Derived>& p)
    {
        this->Eigen::Vector4f::operator=(p);
        return *this;
    }

    float& r() { return x(); }
    float r() const { return x(); }

    float& g() { return y(); }
    float g() const { return y(); }

    float& b() { return z(); }
    float b() const { return z(); }

    float& a() { return coeffRef(3); }
    float a() const { return coeff(3); }

    Eigen::Vector3f to_vec3() const { return Eigen::Vector3f(x(), y(), z()); }
    Eigen::Vector4f to_vec4() const { return Eigen::Vector4f(x(), y(), z(), a()); }

    Color operator+(const float v) const { return Color(r() + v, g() + v, b() + v, a()); }
    Color operator-(const float v) const { return Color(r() - v, g() - v, b() - v, a()); }
    Color operator/(const float v) const { return Color(r() / v, g() / v, b() / v, a()); }
    Color operator*(const float v) const { return Color(r() * v, g() * v, b() * v, a()); }

    Color operator+(const Color c) const
    {
        return Color(r() + c.r(), g() + c.g(), b() + c.b(), std::max(a(), c.a()));
    }
    Color operator-(const Color c) const
    {
        return Color(r() - c.r(), g() - c.g(), b() - c.b(), std::max(a(), c.a()));
    }

    /**
     * Clamps the color
     */
    void clamp()
    {
        if (r() < 0.0f) r() = 0.0f;
        if (r() > 1.0f) r() = 1.0f;
        if (g() < 0.0f) g() = 0.0f;
        if (g() > 1.0f) g() = 1.0f;
        if (b() < 0.0f) b() = 0.0f;
        if (b() > 1.0f) b() = 1.0f;
    }

    Color clamped() const
    {
        return Color(
            std::max(std::min(r(), 1.0f), 0.0f),
            std::max(std::min(g(), 1.0f), 0.0f),
            std::max(std::min(b(), 1.0f), 0.0f),
            std::max(std::min(a(), 1.0f), 0.0f));
    }

    inline bool is_white() { return (r() + g() + b()) >= 3; }

    inline bool is_black() { return (r() == 0 && g() == 0 && b() == 0); }

    float distance(const Color c) const
    {
        return std::abs(c.r() - r()) + std::abs(c.g() - g()) + std::abs(c.b() - b());
    }

    /**
     * Get a random color.
     *
     * @param[in,out] urbg  C++11 random number generator.
     *
     * @tparam        URBG  RNG type.
     *
     * @return        Color.
     */
    template <typename URBG>
    static const Color random_from(URBG&& urbg)
    {
        // static float tau = 0.5f;
        //tau += (1.25f + (float)rand() / RAND_MAX);
        std::uniform_real_distribution<float> dist(0.f, 2.f * lagrange::internal::pi);
        float tau = dist(urbg);

        const float value = static_cast<float>(lagrange::internal::pi) / 3.0f;

        float center = 0.3f; // magic numbers!
        float width = 0.3f;
        return Color(
            std::sin(tau + 0.0f * value) * width + center,
            std::sin(tau + 2.0f * value) * width + center,
            std::sin(tau + 4.0f * value) * width + center);
    }

    // given a number, will always return the same color for the same number, and will produce
    // reasonably different colors for different numbers.
    static const Color random(int i)
    {
        float tau = (float)i;
        float value = (float)lagrange::internal::pi / 3;
        float center = 0.3f;
        float width = 0.3f;
        return Color(
            std::sin(tau + 0.0f * value) * width + center,
            std::sin(tau + 2.0f * value) * width + center,
            std::sin(tau + 4.0f * value) * width + center);
    }

    static Color integer_to_color(int i)
    {
        int r = (i & 0x000000FF);
        int g = (i & 0x0000FF00) >> 8;
        int b = (i & 0x00FF0000) >> 16;
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    }
    int to_integer() const
    {
        int r = int(x() * 255.0f);
        int g = int(y() * 255.0f) << 8;
        int b = int(z() * 255.0f) << 16;
        return (r | g) | b;
    }
};
} // namespace ui
} // namespace lagrange
