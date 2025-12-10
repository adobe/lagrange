/*
 * Copyright 2025 Adobe. All rights reserved.
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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <RectangleBinPack/GuillotineBinPack.h>
#include <RectangleBinPack/Rect.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Core>

#include <exception>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace lagrange::packing {

template <typename T>
bool product_will_overflow(T a, T b)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type");

    if constexpr (std::is_signed<T>::value) {
        using Limits = std::numeric_limits<T>;

        if (a > 0) {
            if (b > 0) {
                return a > Limits::max() / b;
            } else if (b < 0) {
                return b < Limits::min() / a;
            }
        } else if (a < 0) {
            if (b > 0) {
                return a < Limits::min() / b;
            } else if (b < 0) {
                return a < Limits::max() / b;
            }
        }
        return false; // one of them is zero
    } else {
        // Unsigned case
        if (b == 0) return false;
        return a > std::numeric_limits<T>::max() / b;
    }
}

class PackingFailure : public std::runtime_error
{
public:
    PackingFailure(const std::string& what)
        : std::runtime_error(what)
    {}
};

///
/// Pack boxes into a square canvas.
///
/// @param[in] bbox_mins  The minimum coordinates of the boxes.
/// @param[in] bbox_maxs  The maximum coordinates of the boxes.
/// @param[in] allow_rotation  Whether to allow box to rotate by 90 degree when packing.
/// @param[in] margin  Minimum allowed distance between two boxes relative to the canvas.
///
/// @return A tuple containing:
/// - The centers of the packed boxes.
/// - A vector indicating whether each box was rotated.
/// - The size of the canvas used for packing.
///
template <typename Scalar>
std::tuple<Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>, std::vector<bool>, Scalar>
pack_boxes(
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>& bbox_mins,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>& bbox_maxs,
    bool allow_rotation,
    float margin)
{
#ifdef RECTANGLE_BIN_PACK_OSS
    using Int = int;
#else
    using Int = ::rbp::Int;
#endif

    la_runtime_assert(bbox_mins.array().isFinite().all());
    la_runtime_assert(bbox_maxs.array().isFinite().all());
    la_runtime_assert(bbox_mins.rows() == bbox_maxs.rows());
    const auto num_boxes = bbox_mins.rows();
    if (num_boxes == 0) {
        return std::make_tuple(
            Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>(0, 2),
            std::vector<bool>(),
            1);
    }

    const Scalar max_box_length = (bbox_maxs - bbox_mins).maxCoeff();
    constexpr Int MAX_AREA = std::numeric_limits<Int>::max();
    constexpr Int RESOLUTION = 1 << 12;
    Int max_canvas_size = RESOLUTION;
    Int min_canvas_size = RESOLUTION;

    // The scale factor normalizes the boxes such that the largest box fits into a
    // canvas of size max_canvas_size.
    const Scalar scale =
        max_box_length > safe_cast<Scalar>(1e-12) ? max_box_length / max_canvas_size : 1;
    la_runtime_assert(std::isfinite(scale));
    logger().trace("Scale: {}", scale);
    la_debug_assert(product_will_overflow<Int>(2, MAX_AREA));

    std::vector<::rbp::RectSize> boxes;
    boxes.reserve(num_boxes);
    for (auto i : range(num_boxes)) {
        boxes.emplace_back();
        auto& box = boxes.back();
        if (bbox_maxs(i, 0) < bbox_mins(i, 0) || bbox_maxs(i, 1) < bbox_mins(i, 1)) {
            // Invalid bounding box.
            logger().warn("Skipping invalid bounding box (index {})!", i);
            box.width = 0;
            box.height = 0;
        } else {
            box.width = safe_cast<Int>(std::ceil((bbox_maxs(i, 0) - bbox_mins(i, 0)) / scale));
            box.height = safe_cast<Int>(std::ceil((bbox_maxs(i, 1) - bbox_mins(i, 1)) / scale));
            la_debug_assert(!product_will_overflow<Int>(box.width, box.height));
        }
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> centers(num_boxes, 2);
    std::vector<bool> rotated(num_boxes);

    auto pack = [&](int L, bool trial) -> bool {
        la_debug_assert(!product_will_overflow<Int>(L, L));
#ifdef RECTANGLE_BIN_PACK_OSS
        if (!allow_rotation) {
            logger().warn(
                "Disabling rotation is not supported with this version of RectangleBinPack!");
        }
        rbp::GuillotineBinPack packer(L, L);
#else
        rbp::GuillotineBinPack packer(L, L, allow_rotation);
#endif
        Int int_margin = std::max<Int>(2, static_cast<Int>(std::ceil(margin * L)));
        for (auto i : range(num_boxes)) {
            auto rect = packer.Insert(
                boxes[i].width + int_margin,
                boxes[i].height + int_margin,
                false, // Perform empty space merging for defragmentation.
                rbp::GuillotineBinPack::FreeRectChoiceHeuristic::RectBestAreaFit,
                rbp::GuillotineBinPack::GuillotineSplitHeuristic::SplitMinimizeArea);
            if (rect.width == 0 || rect.height == 0) {
                return false;
            }
        }
        const auto& packed_rect = packer.GetUsedRectangles();
        logger().debug("num packed rectangles {}, expecting {}", packed_rect.size(), num_boxes);
        if (static_cast<decltype(num_boxes)>(packed_rect.size()) != num_boxes) return false;

        if (!trial) {
            for (auto i : range(num_boxes)) {
                const auto& r = packed_rect[i];
                rotated[i] = r.width != boxes[i].width + int_margin;
                la_debug_assert(allow_rotation || !rotated[i]);
                centers(i, 0) = (r.x + r.width * 0.5f) * scale;
                centers(i, 1) = (r.y + r.height * 0.5f) * scale;
            }
        }
        return true;
    };

    logger().trace("Minimum canvas size: {}", min_canvas_size);
    logger().trace("Maximum canvas size: {}", max_canvas_size);
    // Find max_canvas_size large enough to fit all boxes.
    while (!pack(max_canvas_size, true)) {
        min_canvas_size = max_canvas_size;
        max_canvas_size *= 2;
        if (MAX_AREA / max_canvas_size <= max_canvas_size) {
            // Ops, run out of bound.
            throw PackingFailure("Cannot pack even with canvas at max area!");
        }
    }
    logger().trace("Minimum canvas size: {}", min_canvas_size);
    logger().trace("Maximum canvas size: {}", max_canvas_size);
    la_runtime_assert(max_canvas_size > 0);
    // Binary search for the smallest max_canvas_size that fits.
    while (min_canvas_size < max_canvas_size) {
        const auto l = (min_canvas_size + max_canvas_size) / 2;
        if (l == min_canvas_size) {
            break;
        }
        if (pack(l, true)) {
            max_canvas_size = l;
        } else {
            min_canvas_size = l;
        }
    }
    la_runtime_assert(max_canvas_size > 0);
    bool r = pack(max_canvas_size, false);
    la_runtime_assert(r);
    logger().trace("Minimum canvas size: {}", min_canvas_size);
    logger().trace("Maximum canvas size: {}", max_canvas_size);

    return std::make_tuple(centers, rotated, static_cast<Scalar>(max_canvas_size) * scale);
}

} // namespace lagrange::packing
