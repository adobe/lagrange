/*
 * Source: https://github.com/mkazhdan/TextureSignalProcessing/blob/master/include/Src/Padding.h
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018, Fabian Prada and Michael Kazhdan All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * Neither the name of the Johns Hopkins University nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file has been modified by Adobe.
 *
 * All modifications are Copyright 2025 Adobe.
 */
#pragma once

#include <lagrange/Logger.h>
#include <lagrange/utils/span.h>

// Include before any TextureSignalProcessing header to override their threadpool implementation.
#include "ThreadPool.h"
#define MULTI_THREADING_INCLUDED
using namespace lagrange::texproc::threadpool;

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Misha/RegularGrid.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::texproc {

// A structure for computing and (un)setting the padding needed to ensure that texture coordinates
// fall within the rectangle defined by the _centers_ of the corner texels. Given a texture
// coordinate (s) indexing a texture map of width W, we have:
//.   s -> W*s
// Offsetting W -> W+D we want the associated texture coordinate t to satisfy:
//.   (W+D)*t = D + W*s
//.   t = ( W*s + D ) / (W+D)
class Padding
{
public:
    template <typename Scalar>
    using Vector2 = std::array<Scalar, 2>;

    template <unsigned int N, typename T>
    using RegularGrid = MishaK::RegularGrid<N, T>;

private:
    unsigned int left = 0;
    unsigned int right = 0;
    unsigned int bottom = 0;
    unsigned int top = 0;

public:
    unsigned int width() const { return left + right; }
    unsigned int height() const { return bottom + top; }

    template <typename Scalar>
    static Padding
    init(unsigned int width, unsigned int height, span<const std::array<Scalar, 2>> texcoords)
    {
        Padding padding;
        Vector2<Scalar> pix_min_corner{Scalar(0.5) / width, Scalar(0.5) / height};
        Vector2<Scalar> pix_max_corner{Scalar(width - 0.5) / width, Scalar(height - 0.5) / height};

        Vector2<Scalar> tex_min_corner{texcoords[0][0], texcoords[0][1]};
        Vector2<Scalar> tex_max_corner{texcoords[0][0], texcoords[0][1]};
        for (size_t i = 0; i < texcoords.size(); i++) {
            for (int c = 0; c < 2; c++) {
                tex_min_corner[c] = std::min<Scalar>(tex_min_corner[c], texcoords[i][c]);
            }
            for (int c = 0; c < 2; c++) {
                tex_max_corner[c] = std::max<Scalar>(tex_max_corner[c], texcoords[i][c]);
            }
        }
        std::swap(tex_min_corner[1], tex_max_corner[1]);
        tex_min_corner[1] = 1.0 - tex_min_corner[1];
        tex_max_corner[1] = 1.0 - tex_max_corner[1];
        logger().debug(
            "Texture coordinate bounding box : Min ({}, {}). Max ({}, {}). SafeMin ({}, {}). "
            "SafeMax ({}, {})",
            tex_min_corner[0],
            tex_min_corner[1],
            tex_max_corner[0],
            tex_max_corner[1],
            pix_min_corner[0],
            pix_min_corner[1],
            pix_max_corner[0],
            pix_max_corner[1]);

        padding.left = tex_min_corner[0] < pix_min_corner[0]
                           ? (int)ceil((pix_min_corner[0] - tex_min_corner[0]) * width)
                           : 0;
        padding.bottom = tex_min_corner[1] < pix_min_corner[1]
                             ? (int)ceil((pix_min_corner[1] - tex_min_corner[1]) * height)
                             : 0;

        padding.right = tex_max_corner[0] > pix_max_corner[0]
                            ? (int)ceil((tex_max_corner[0] - pix_max_corner[0]) * width)
                            : 0;
        padding.top = tex_max_corner[1] > pix_max_corner[1]
                          ? (int)ceil((tex_max_corner[1] - pix_max_corner[1]) * height)
                          : 0;

        // Make image dimensions multiples of 8 (Hardware texture mapping seems to fail if not)
        {
            int new_width = width + padding.left + padding.right;
            int new_height = height + padding.bottom + padding.top;

            int padded_width = 8 * (((new_width - 1) / 8) + 1);
            int padded_height = 8 * (((new_height - 1) / 8) + 1);
            padding.left += (padded_width - new_width);
            padding.bottom += (padded_height - new_height);
        }

        if (padding.width() || padding.height()) {
            logger().debug(
                "Padding applied : Left {}. Right {}. Bottom {}. Top {}.",
                padding.left,
                padding.right,
                padding.bottom,
                padding.top);
        } else {
            logger().debug("No padding required!");
        }

        return padding;
    }

    // Add the padding to an image (set new texel values to closest boundary texel)
    // [WARNING] Assuming the image dimensions match those used to define the object
    template <typename DataType>
    void pad(RegularGrid<2, DataType>& im) const
    {
        if (!(left || right || bottom || top)) return;

        unsigned int new_width = im.res(0) + left + right;
        unsigned int new_height = im.res(1) + bottom + top;

        RegularGrid<2, DataType> new_im;
        new_im.resize(new_width, new_height);
        for (unsigned int i = 0; i < new_width; i++)
            for (unsigned int j = 0; j < new_height; j++) {
                unsigned int ni =
                    std::min<int>(std::max<int>(0, (int)i - (int)left), im.res(0) - 1);
                unsigned int nj =
                    std::min<int>(std::max<int>(0, (int)j - (int)bottom), im.res(1) - 1);
                new_im(i, j) = im(ni, nj);
            }
        im = new_im;
    }

    // Remove the padding from an image
    template <class DataType>
    void unpad(RegularGrid<2, DataType>& im) const
    {
        if (!(left || right || bottom || top)) return;

        unsigned int output_width = im.res(0) - left - right;
        unsigned int output_height = im.res(1) - bottom - top;
        RegularGrid<2, DataType> new_im;
        new_im.resize(output_width, output_height);
        for (unsigned int i = 0; i < output_width; i++) {
            for (unsigned int j = 0; j < output_height; j++) {
                new_im(i, j) = im(left + i, bottom + j);
            }
        }
        im = new_im;
    }

    template <typename Scalar>
    void pad(int width, int height, span<std::array<Scalar, 2>> texcoords) const
    {
        if (!(left || right || bottom || top)) return;

        int new_width = width + left + right;
        int new_height = height + bottom + top;

        for (size_t i = 0; i < texcoords.size(); i++) {
            texcoords[i][0] = (texcoords[i][0] * width + (Scalar)(left)) / new_width;
            texcoords[i][1] =
                1.0 - ((1.0 - texcoords[i][1]) * height + (Scalar)(bottom)) / new_height;
        }
    }
};

} // namespace lagrange::texproc
