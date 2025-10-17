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
#pragma once

#include <lagrange/image/ImageType.h>
#include <tbb/cache_aligned_allocator.h>
#include <Eigen/Eigen>
#include <memory>
#include <vector>

namespace lagrange {
namespace image {

class ImageStorage
{
public:
    template <typename T>
    using aligned_allocator = tbb::cache_aligned_allocator<T>;
    template <typename T>
    using aligned_vector = std::vector<T, aligned_allocator<T>>;

    inline ImageStorage(size_t width, size_t height, size_t alignment);
    inline ImageStorage(size_t width, size_t height, size_t stride, unsigned char* data);

    inline ImageStorage(const ImageStorage& other) { *this = other; };
    ImageStorage(ImageStorage&& other) noexcept { *this = other; };

    virtual ~ImageStorage() {};

public:
    inline ImageStorage& operator=(const ImageStorage& other);
    inline ImageStorage& operator=(ImageStorage&& other) noexcept;
    inline void clear_buffer(unsigned char ch);

protected:
    inline void reset();
    inline bool resize(size_t width, size_t height, size_t alignment);

public:
    auto get_full_size() const { return m_full_size; }
    auto get_full_stride() const { return m_full_stride; }
    const auto* data() const { return m_data_ptr; }
    auto* data() { return m_data_ptr; }

protected:
    aligned_vector<unsigned char> m_buffer;
    unsigned char* m_buffer_weak_ptr = nullptr; // for m_buffer does not owned by the storage
    unsigned char* m_data_ptr = nullptr; // data pointer to either m_buffer or m_buffer_weak_ptr;

    Eigen::Matrix<size_t, 2, 1> m_full_size;
    size_t m_full_stride;
};


inline ImageStorage::ImageStorage(size_t width, size_t height, size_t alignment)
{
    if (!resize(width, height, alignment)) {
        throw std::runtime_error("ImageStorage::ImageStorage, cannot resize!");
    }
}
inline ImageStorage::ImageStorage(size_t width, size_t height, size_t stride, unsigned char* data)
{
    if (0 == width || 0 == height || 0 == stride || width > stride || nullptr == data) {
        throw std::runtime_error(
            "ImageStorage::ImageStorage, width or height or stride or data is invalid!");
    }
    m_full_size = {width, height};
    m_full_stride = stride;
    m_buffer_weak_ptr = data;
    m_data_ptr = m_buffer_weak_ptr;
}

inline ImageStorage& ImageStorage::operator=(const ImageStorage& other)
{
    m_full_size = other.m_full_size;
    m_full_stride = other.m_full_stride;
    m_buffer = other.m_buffer;
    m_buffer_weak_ptr = other.m_buffer_weak_ptr;
    m_data_ptr = other.m_data_ptr;
    return *this;
}
inline ImageStorage& ImageStorage::operator=(ImageStorage&& other) noexcept
{
    m_full_size = other.m_full_size;
    m_full_stride = other.m_full_stride;
    m_buffer = std::move(other.m_buffer);
    m_buffer_weak_ptr = other.m_buffer_weak_ptr;
    m_data_ptr = other.m_data_ptr;
    return *this;
}

inline void ImageStorage::clear_buffer(unsigned char ch)
{
    if (m_data_ptr) {
        std::fill(m_data_ptr, m_data_ptr + m_full_size(0) * m_full_size(1), ch);
    }
}

inline void ImageStorage::reset()
{
    m_buffer.clear();
    m_full_size.setZero();
    m_full_stride = 0;
    m_buffer_weak_ptr = nullptr;
    m_data_ptr = nullptr;
}
inline bool ImageStorage::resize(size_t width, size_t height, size_t alignment)
{
    if (0 == width || 0 == height || 0 == alignment || 0 != ((alignment - 1) & alignment)) {
        reset();
        return false;
    } else {
        m_full_size = {width, height};
        m_full_stride = ((width - 1) / alignment + 1) * alignment;
        m_buffer.clear();
        m_buffer.resize(m_full_stride * height);
        m_buffer_weak_ptr = nullptr;
        m_data_ptr = m_buffer.data();
        return true;
    }
}
} // namespace image
} // namespace lagrange
