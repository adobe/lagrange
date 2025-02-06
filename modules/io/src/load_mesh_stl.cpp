/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/api.h>
#include <lagrange/io/load_mesh_stl.h>
#include <lagrange/utils/assert.h>

#include "stitch_mesh.h"

namespace lagrange::io {

namespace {

bool is_binary(std::istream& input_stream)
{
    // Read the first 80 bytes and number of triangles
    char header[80] = {0};
    input_stream.read(header, 80);

    uint32_t num_triangles;
    input_stream.read(reinterpret_cast<char*>(&num_triangles), sizeof(num_triangles));

    // Calculate expected size for binary format
    input_stream.seekg(0, std::ios::end);
    std::streampos file_size = input_stream.tellg();
    input_stream.seekg(0, std::ios::beg);

    // Expected size = 80 bytes header + 4 bytes number of triangles + 50 bytes per triangle
    std::streampos expected_size = 84 + static_cast<std::streampos>(50 * num_triangles);
    return file_size == expected_size;
}

template <typename Scalar>
std::vector<Scalar> load_stl_ascii(std::istream& input_stream)
{
    std::vector<Scalar> triangles;
    std::string line;

    while (std::getline(input_stream, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "facet") {
            // Normal field in STL format is rarely used. We ignore it.
            iss.ignore(256, '\n'); // Skip the normal line
        } else if (token == "vertex") {
            Scalar x, y, z;
            iss >> x >> y >> z;
            triangles.insert(triangles.end(), {x, y, z});
        }
    }

    return triangles;
}

std::vector<float> load_stl_binary(std::istream& input_stream)
{
    // Skip header and extract num_triangles
    input_stream.seekg(80, std::ios::beg);
    uint32_t num_triangles;
    input_stream.read(reinterpret_cast<char*>(&num_triangles), sizeof(num_triangles));

    std::vector<float> triangles;
    triangles.reserve(num_triangles * 3);
    char buffer[50];
    while (input_stream.read(buffer, 50)) {
        float normal[3];
        float v1[3];
        float v2[3];
        float v3[3];
        uint16_t attr;
        std::memcpy(normal, buffer + 0, 3 * sizeof(float));
        std::memcpy(v1, buffer + 12, 3 * sizeof(float));
        std::memcpy(v2, buffer + 24, 3 * sizeof(float));
        std::memcpy(v3, buffer + 36, 3 * sizeof(float));
        std::memcpy(&attr, buffer + 48, sizeof(uint16_t));
        triangles.insert(triangles.end(), v1, v1 + 3);
        triangles.insert(triangles.end(), v2, v2 + 3);
        triangles.insert(triangles.end(), v3, v3 + 3);
    }
    return triangles;
}

} // namespace

template <typename MeshType>
MeshType load_mesh_stl(std::istream& input_stream, [[maybe_unused]] const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    MeshType mesh;

    if (is_binary(input_stream)) {
        auto triangles = load_stl_binary(input_stream);
        Index num_vertices = static_cast<Index>(triangles.size() / 3);
        Index num_triangles = num_vertices / 3;

        if constexpr (std::is_same_v<Scalar, float>) {
            mesh.add_vertices(num_vertices, {triangles.data(), triangles.size()});
        } else {
            mesh.add_vertices(num_vertices, [&](Index vid, span<Scalar> coords) {
                coords[0] = static_cast<Scalar>(triangles[3 * vid]);
                coords[1] = static_cast<Scalar>(triangles[3 * vid + 1]);
                coords[2] = static_cast<Scalar>(triangles[3 * vid + 2]);
            });
        }
        mesh.add_triangles(num_triangles, [](Index fid, span<Index> t) {
            t[0] = 3 * fid;
            t[1] = 3 * fid + 1;
            t[2] = 3 * fid + 2;
        });
    } else {
        auto triangles = load_stl_ascii<Scalar>(input_stream);
        Index num_vertices = static_cast<Index>(triangles.size() / 3);
        Index num_triangles = num_vertices / 3;

        mesh.add_vertices(num_vertices, {triangles.data(), triangles.size()});
        mesh.add_triangles(num_triangles, [](Index fid, span<Index> t) {
            t[0] = 3 * fid;
            t[1] = 3 * fid + 1;
            t[2] = 3 * fid + 2;
        });
    }

    // Always stitch mesh for STL (i.e. triangle soup) format.
    stitch_mesh(mesh);

    return mesh;
}

template <typename MeshType>
MeshType load_mesh_stl(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename, std::ios::binary);
    la_runtime_assert(fin.good(), fmt::format("Unable to open file {}", filename.string()));
    return load_mesh_stl<MeshType>(fin, options);
}

#define LA_X_load_mesh_stl(_, S, I)                                                                \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_stl(std::istream&, const LoadOptions& options); \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_stl(                                            \
        const fs::path& filename,                                                                  \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_stl, 0)

} // namespace lagrange::io
