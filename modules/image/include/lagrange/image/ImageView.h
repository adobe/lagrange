/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/image/ImageStorage.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <vector>

namespace lagrange {
namespace image {

class ImageViewBase
{
protected:
    ImageViewBase() = default;
    virtual ~ImageViewBase() = default;

public: // virtual methods
    virtual ImagePrecision get_precision() const = 0;
    virtual ImageChannel get_channel() const = 0;
    virtual bool is_compact_row() const = 0;
    virtual bool is_compact() const = 0;
    virtual const void* get_data() const = 0;
    virtual void* get_data() = 0;

public: // non-virtual methods
    Eigen::Matrix<size_t, 2, 1> get_view_size() const { return m_view_size; }
    Eigen::Matrix<size_t, 2, 1> get_view_stride_in_byte() const { return m_view_stride_in_byte; }
    Eigen::Matrix<size_t, 2, 1> get_view_offset_in_byte() const { return m_view_offset_in_byte; }
    std::shared_ptr<ImageStorage> get_storage() { return m_storage; }

protected: // non-template variables
    Eigen::Matrix<size_t, 2, 1> m_view_size;
    Eigen::Matrix<size_t, 2, 1> m_view_stride_in_byte;
    Eigen::Matrix<size_t, 2, 1> m_view_offset_in_byte;
    std::shared_ptr<ImageStorage> m_storage;
};

template <typename T>
class ImageView : public ImageViewBase
{
public:
    ImageView();
    ImageView(size_t width, size_t height, size_t alignment);
    ImageView(
        std::shared_ptr<ImageStorage> _storage,
        const size_t width,
        const size_t height,
        const size_t stride_0_in_byte = sizeof(T),
        const size_t stride_1_in_row = 1,
        const size_t offset_0_in_byte = 0,
        const size_t offset_1_in_row = 0);
    ImageView(const ImageView<T>& other);
    ImageView(ImageView<T>&& other);
    virtual ~ImageView() = default;

public:
    ImageView<T>& operator=(const ImageView<T>& other);
    ImageView<T>& operator=(ImageView<T>&& other);

public:
    void reset();
    bool resize(size_t width, size_t height, size_t alignment);

public:
    bool view(
        std::shared_ptr<ImageStorage> _storage,
        const size_t width,
        const size_t height,
        const size_t stride_0_in_byte = sizeof(T),
        const size_t stride_1_in_row = 1,
        const size_t offset_0_in_byte = 0,
        const size_t offset_1_in_row = 0);

    ImageStorage::aligned_vector<unsigned char> pack() const;
    bool unpack(const ImageStorage::aligned_vector<unsigned char>& buf);

    template <typename S, typename CONVERTOR>
    bool convert_from(const ImageView<S>& other, size_t alignment, const CONVERTOR& convertor);
    template <typename S>
    bool convert_from(const ImageView<S>& other, size_t alignment);

    void clear(const T val);

public:
    // x is col, y is row
    T& operator()(size_t x, size_t y);
    const T& operator()(size_t x, size_t y) const;

public: // for interfaces of ImageViewBase
    virtual ImagePrecision get_precision() const final { return ImageTraits<T>::precision; };
    virtual ImageChannel get_channel() const final { return ImageTraits<T>::channel; };
    virtual bool is_compact_row() const final { return sizeof(T) == m_view_stride_in_byte(0); }
    virtual bool is_compact() const final
    {
        return sizeof(T) * m_view_size(0) == m_view_stride_in_byte(1);
    }
    virtual const void* get_data() const final
    {
        return reinterpret_cast<const void*>(&(operator()(0, 0)));
    };
    virtual void* get_data() final { return reinterpret_cast<void*>(&(operator()(0, 0))); };
};

template <typename T>
ImageView<T>::ImageView()
{
    reset();
}
template <typename T>
ImageView<T>::ImageView(size_t width, size_t height, size_t alignment)
{
    if (!resize(width, height, alignment)) {
        throw std::runtime_error("ImageView::ImageView, cannot resize!");
    }
}
template <typename T>
ImageView<T>::ImageView(
    std::shared_ptr<ImageStorage> _storage,
    size_t width,
    size_t height,
    size_t stride_0_in_byte,
    size_t stride_1_in_row,
    size_t offset_0_in_byte,
    size_t offset_1_in_row)
{
    if (!view(
            _storage,
            width,
            height,
            stride_0_in_byte,
            stride_1_in_row,
            offset_0_in_byte,
            offset_1_in_row)) {
        throw std::runtime_error("ImageView::ImageView, cannot construct from ImageStorage!");
    }
}
template <typename T>
ImageView<T>::ImageView(const ImageView<T>& other)
{
    *this = other;
}
template <typename T>
ImageView<T>::ImageView(ImageView<T>&& other)
{
    *this = other;
}

template <typename T>
ImageView<T>& ImageView<T>::operator=(const ImageView<T>& other)
{
    m_view_size = other.m_view_size;
    m_view_stride_in_byte = other.m_view_stride_in_byte;
    m_view_offset_in_byte = other.m_view_offset_in_byte;
    m_storage = other.m_storage;
    return *this;
}
template <typename T>
ImageView<T>& ImageView<T>::operator=(ImageView<T>&& other)
{
    m_view_size = other.m_view_size;
    m_view_stride_in_byte = other.m_view_stride_in_byte;
    m_view_offset_in_byte = other.m_view_offset_in_byte;
    m_storage = std::move(other.m_storage);
    return *this;
}

template <typename T>
void ImageView<T>::reset()
{
    m_storage.reset();
    m_view_size.setZero();
    m_view_stride_in_byte.setZero();
    m_view_offset_in_byte.setZero();
}
template <typename T>
bool ImageView<T>::resize(size_t width, size_t height, size_t alignment)
{
    if (0 == width || 0 == height) {
        reset();
        return false;
    } else {
        m_storage = std::make_shared<ImageStorage>(width * sizeof(T), height, alignment);
        m_view_size = {width, height};
        m_view_stride_in_byte(0) = sizeof(T);
        m_view_stride_in_byte(1) = m_storage->get_full_stride();
        m_view_offset_in_byte.setZero();
        return true;
    }
}

template <typename T>
bool ImageView<T>::view(
    std::shared_ptr<ImageStorage> _storage,
    size_t width,
    size_t height,
    size_t stride_0_in_byte,
    size_t stride_1_in_row,
    size_t offset_0_in_byte,
    size_t offset_1_in_row)
{
    if (_storage && 0 < width && 0 < height) {
        auto full_size = _storage->get_full_size();
        auto full_stride = _storage->get_full_stride();
        if (sizeof(T) <= stride_0_in_byte &&
            offset_0_in_byte + stride_0_in_byte * width <= full_size(0) && 0 < stride_1_in_row &&
            offset_1_in_row + stride_1_in_row * height <= full_size(1)) {
            m_storage = _storage;
            m_view_size = {width, height};
            m_view_stride_in_byte = {stride_0_in_byte, stride_1_in_row * full_stride};
            m_view_offset_in_byte = {offset_0_in_byte, offset_1_in_row * full_stride};
            return true;
        }
    }
    reset();
    return false;
}

template <typename T>
ImageStorage::aligned_vector<unsigned char> ImageView<T>::pack() const
{
    ImageStorage::aligned_vector<unsigned char> buf(sizeof(T) * m_view_size(0) * m_view_size(1));
    if (is_compact()) {
        std::copy_n(
            reinterpret_cast<const unsigned char*>(&(operator()(0, 0))),
            buf.size(),
            buf.data());
    } else if (sizeof(T) == m_view_stride_in_byte(0)) {
        auto src = reinterpret_cast<const unsigned char*>(&(operator()(0, 0)));
        auto dst = buf.data();
        auto dst_stride_in_byte_row = sizeof(T) * m_view_size(0);
        for (size_t row = 0; row < m_view_size(1); ++row) {
            std::copy_n(src, dst_stride_in_byte_row, dst);
            src += m_view_stride_in_byte(1);
            dst += dst_stride_in_byte_row;
        }
    } else {
        auto src_row = reinterpret_cast<const unsigned char*>(&(operator()(0, 0)));
        auto dst_row = buf.data();
        auto dst_stride_in_byte_col = sizeof(T);
        auto dst_stride_in_byte_row = sizeof(T) * m_view_size(0);
        for (size_t row = 0; row < m_view_size(1); ++row) {
            auto src_pix = src_row;
            auto dst_pix = dst_row;
            for (size_t col = 0; col < m_view_size(0); ++col) {
                *reinterpret_cast<T*>(dst_pix) = *reinterpret_cast<const T*>(src_pix);
                src_pix += m_view_stride_in_byte(0);
                dst_pix += dst_stride_in_byte_col;
            }
            src_row += m_view_stride_in_byte(1);
            dst_row += dst_stride_in_byte_row;
        }
    }
    return buf;
}
template <typename T>
bool ImageView<T>::unpack(const ImageStorage::aligned_vector<unsigned char>& buf)
{
    if (sizeof(T) * m_view_size(0) * m_view_size(1) != buf.size()) {
        return false;
    }

    if (is_compact()) {
        std::copy_n(buf.data(), buf.size(), reinterpret_cast<unsigned char*>(&(operator()(0, 0))));
    } else if (sizeof(T) == m_view_stride_in_byte(0)) {
        auto dst = reinterpret_cast<unsigned char*>(&(operator()(0, 0)));
        auto src = buf.data();
        auto src_stride_in_byte_row = sizeof(T) * m_view_size(0);
        for (size_t row = 0; row < m_view_size(1); ++row) {
            std::copy_n(src, src_stride_in_byte_row, dst);
            dst += m_view_stride_in_byte(1);
            src += src_stride_in_byte_row;
        }
    } else {
        auto dst_row = reinterpret_cast<unsigned char*>(&(operator()(0, 0)));
        auto src_row = buf.data();
        auto src_stride_in_byte_col = sizeof(T);
        auto src_stride_in_byte_row = sizeof(T) * m_view_size(0);
        for (size_t row = 0; row < m_view_size(1); ++row) {
            auto dst_pix = dst_row;
            auto src_pix = src_row;
            for (size_t col = 0; col < m_view_size(0); ++col) {
                *reinterpret_cast<T*>(dst_pix) = *reinterpret_cast<const T*>(src_pix);
                dst_pix += m_view_stride_in_byte(0);
                src_pix += src_stride_in_byte_col;
            }
            dst_row += m_view_stride_in_byte(1);
            src_row += src_stride_in_byte_row;
        }
    }
    return true;
}

template <typename T>
template <typename S, typename CONVERTOR>
bool ImageView<T>::convert_from(
    const ImageView<S>& other,
    size_t alignment,
    const CONVERTOR& convertor)
{
    const auto other_view_size = other.get_view_size();
    if (!resize(other_view_size(0), other_view_size(1), alignment)) {
        return false;
    }

    const char* src = reinterpret_cast<const char*>(&(other(0, 0)));
    char* dst = reinterpret_cast<char*>(&(operator()(0, 0)));
    const auto other_view_stride_in_byte = other.get_view_stride_in_byte();
    tbb::parallel_for(static_cast<size_t>(0), other_view_size(1), [&](size_t y) {
        const char* src_row = src + other_view_stride_in_byte(1) * y;
        char* dst_row = dst + m_view_stride_in_byte(1) * y;
        for (size_t x = 0; x < other_view_size(0); ++x) {
            const S* src_pix =
                reinterpret_cast<const S*>(src_row + other_view_stride_in_byte(0) * x);
            T* dst_pix = reinterpret_cast<T*>(dst_row + m_view_stride_in_byte(0) * x);
            convertor(*src_pix, *dst_pix);
        }
    });
    return true;
}

template <typename T>
template <typename S>
bool ImageView<T>::convert_from(const ImageView<S>& other, size_t alignment)
{
    return convert_from(other, alignment, convert_image_pixel<S, T>());
}

template <typename T>
void ImageView<T>::clear(const T val)
{
    tbb::parallel_for(static_cast<size_t>(0), m_view_size(1), [&](size_t h) {
        auto* ptr = reinterpret_cast<unsigned char*>(&((*this)(0, h)));
        for (size_t w = 0; w < m_view_size(0); ++w) {
            auto* p = reinterpret_cast<T*>(ptr + w * m_view_stride_in_byte(0));
            *p = val;
        }
    });
}

template <typename T>
T& ImageView<T>::operator()(size_t x, size_t y)
{
    auto total_offset = m_view_offset_in_byte(0) + m_view_offset_in_byte(1) +
                        m_view_stride_in_byte(0) * x + m_view_stride_in_byte(1) * y;
    return *reinterpret_cast<T*>(m_storage->data() + total_offset);
}
template <typename T>
const T& ImageView<T>::operator()(size_t x, size_t y) const
{
    auto total_offset = m_view_offset_in_byte(0) + m_view_offset_in_byte(1) +
                        m_view_stride_in_byte(0) * x + m_view_stride_in_byte(1) * y;
    return *reinterpret_cast<T*>(m_storage->data() + total_offset);
}
} // namespace image
} // namespace lagrange
