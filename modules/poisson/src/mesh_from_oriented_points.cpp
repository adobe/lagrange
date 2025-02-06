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

#include <lagrange/poisson/mesh_from_oriented_points.h>

#include "octree_depth.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/views.h>

// Include before any PoissonRecon header to override their threadpool implementation.
#include "ThreadPool.h"
#define MULTI_THREADING_INCLUDED
using namespace lagrange::poisson::threadpool;

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <PreProcessor.h>
#include <Reconstructors.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::poisson {

namespace {

using ReconScalar = float;
constexpr unsigned int Dim = 3;

template <typename MeshScalar>
struct InputPointStream
    : public PoissonRecon::Reconstructor::InputOrientedSampleStream<ReconScalar, Dim>
{
    // Constructs a stream that contains the specified number of samples
    InputPointStream(span<const MeshScalar> P, span<const MeshScalar> N)
        : m_points(P)
        , m_normals(N)
        , m_current(0)
    {
        la_runtime_assert(
            m_points.size() == m_normals.size(),
            "Number of normals and points don't match");
    }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    void reset() override { m_current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    bool read(PoissonRecon::Point<ReconScalar, Dim>& p, PoissonRecon::Point<ReconScalar, Dim>& n)
        override
    {
        if (m_current * Dim < m_points.size()) {
            for (unsigned int d = 0; d < Dim; d++) {
                p[d] = static_cast<ReconScalar>(m_points[m_current * Dim + d]);
                n[d] = static_cast<ReconScalar>(m_normals[m_current * Dim + d]);
            }
            m_current++;
            return true;
        } else {
            return false;
        }
    }
    bool read(
        unsigned int,
        PoissonRecon::Point<ReconScalar, Dim>& p,
        PoissonRecon::Point<ReconScalar, Dim>& n) override
    {
        return read(p, n);
    }

protected:
    span<const MeshScalar> m_points;
    span<const MeshScalar> m_normals;
    unsigned int m_current;
};

template <typename MeshScalar, typename ValueType>
struct InputPointStreamWithAttribute
    : public PoissonRecon::Reconstructor::
          InputOrientedSampleStream<ReconScalar, Dim, PoissonRecon::Point<ReconScalar>>
{
    // Constructs a stream that contains the specified number of samples
    InputPointStreamWithAttribute(
        span<const MeshScalar> P,
        span<const MeshScalar> N,
        const Attribute<ValueType>& attribute)
        : m_points(P)
        , m_normals(N)
        , m_attribute(attribute)
        , m_num_channels(static_cast<unsigned int>(m_attribute.get_num_channels()))
        , m_current(0)
    {
        la_runtime_assert(
            m_points.size() == m_normals.size(),
            "Number of normals and points don't match");
        la_runtime_assert(
            m_points.size() / Dim == m_attribute.get_num_elements(),
            "Number of attribute elements doesn't match number of vertices");
    }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    void reset() override { m_current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    bool read(
        PoissonRecon::Point<ReconScalar, Dim>& p,
        PoissonRecon::Point<ReconScalar, Dim>& n,
        PoissonRecon::Point<ReconScalar>& data) override
    {
        if (m_current * Dim < m_points.size()) {
            // Copy the positions and the normals
            for (unsigned int d = 0; d < Dim; d++) {
                p[d] = static_cast<ReconScalar>(m_points[m_current * Dim + d]);
                n[d] = static_cast<ReconScalar>(m_normals[m_current * Dim + d]);
            }

            // Copy the attribute data
            auto row = m_attribute.get_row(m_current);
            for (unsigned int c = 0; c < m_num_channels; c++) {
                data[c] = static_cast<ReconScalar>(row[c]);
            }

            m_current++;
            return true;
        } else {
            return false;
        }
    }
    bool read(
        unsigned int,
        PoissonRecon::Point<ReconScalar, Dim>& p,
        PoissonRecon::Point<ReconScalar, Dim>& n,
        PoissonRecon::Point<ReconScalar>& data) override
    {
        return read(p, n, data);
    }

protected:
    span<const MeshScalar> m_points;
    span<const MeshScalar> m_normals;
    const Attribute<ValueType>& m_attribute;
    unsigned int m_num_channels;
    unsigned int m_current;
};

// A stream into which we can write polygons of the form std::vector< node_index_type >
template <typename Scalar, typename Index>
struct OutputTriangleStream : public PoissonRecon::Reconstructor::OutputFaceStream<2>
{
    // Construct a stream that adds polygons to the vector of polygons
    OutputTriangleStream(SurfaceMesh<Scalar, Index>& mesh)
        : m_mesh(mesh)
        , m_triangles(ThreadPool::NumThreads())
    {}

    // Override the pure abstract method from OutputPolygonStream
    size_t write(const std::vector<PoissonRecon::node_index_type>&) override
    {
        throw std::runtime_error("Should not be called");
    }

    size_t write(unsigned int thread, const std::vector<PoissonRecon::node_index_type>& polygon)
        override
    {
        la_runtime_assert(polygon.size() == Dim, "Expected triangular face");
        size_t idx = m_size++;
        m_triangles[thread].emplace_back(
            static_cast<Index>(polygon[0]),
            static_cast<Index>(polygon[1]),
            static_cast<Index>(polygon[2]));
        return idx;
    }

    size_t size() const override { return m_size; }

    void finalize()
    {
        // TODO: Reserve/allocate in advance
        for (unsigned int i = 0; i < m_triangles.size(); i++) {
            for (size_t j = 0; j < m_triangles[i].size(); j++) {
                m_mesh.add_triangle(
                    m_triangles[i][j][0],
                    m_triangles[i][j][1],
                    m_triangles[i][j][2]);
            }
        }
    }

protected:
    struct TriangleIndices
    {
        Index idx[3];
        Index& operator[](unsigned int v) { return idx[v]; }
        const Index& operator[](unsigned int v) const { return idx[v]; }
        TriangleIndices(Index v1 = 0, Index v2 = 0, Index v3 = 0)
        {
            idx[0] = v1, idx[1] = v2, idx[2] = v3;
        }
    };

    SurfaceMesh<Scalar, Index>& m_mesh;
    std::atomic<size_t> m_size = 0;
    std::vector<std::vector<TriangleIndices>> m_triangles;
};

// A stream into which we can write the output vertices of the extracted mesh
template <typename Scalar, typename Index, bool OutputVertexDepth>
struct OutputVertexStream
    : public PoissonRecon::Reconstructor::OutputLevelSetVertexStream<ReconScalar, Dim>
{
    // Construct a stream that adds vertices into the coordinates
    OutputVertexStream(SurfaceMesh<Scalar, Index>& mesh)
        : m_mesh(mesh)
    {
        static_assert(!OutputVertexDepth, "[ERROR] Expected output vertex depth attribute");
        m_vertices.resize(ThreadPool::NumThreads());
    }

    OutputVertexStream(SurfaceMesh<Scalar, Index>& mesh, AttributeId vertex_depth_attribute_id)
        : m_mesh(mesh)
        , m_vertex_depth_attribute(
              &m_mesh.template ref_attribute<Scalar>(vertex_depth_attribute_id))
    {
        static_assert(OutputVertexDepth, "[ERROR] Did not expect output vertex depth attribute");
        m_vertices.resize(ThreadPool::NumThreads());
    }

    // Override the pure abstract method from Reconstructor::OutputVertexStream< Scalar , Dim >
    size_t write(
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const ReconScalar&) override
    {
        throw std::runtime_error("Should not be called");
    }

    size_t write(
        unsigned int thread,
        const PoissonRecon::Point<ReconScalar, Dim>& p,
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const ReconScalar& v_depth) override
    {
        size_t idx = m_size++;
        std::pair<size_t, VertexInfo> v_info;
        v_info.first = idx;
        v_info.second.pos = p;
        v_info.second.depth = v_depth;
        m_vertices[thread].push_back(v_info);
        return idx;
    }

    size_t size() const override { return m_size; }

    void finalize()
    {
        using StreamType = PoissonRecon::InputIndexedDataStream<VertexInfo>;
        using VectorStreamType = PoissonRecon::VectorBackedInputIndexedDataStream<VertexInfo>;

        std::vector<std::unique_ptr<StreamType>> input_streams_owner(m_vertices.size());
        std::vector<StreamType*> input_streams(m_vertices.size());
        for (unsigned int i = 0; i < m_vertices.size(); i++) {
            input_streams_owner[i] = std::make_unique<VectorStreamType>(m_vertices[i]);
            input_streams[i] = input_streams_owner[i].get();
        }
        PoissonRecon::InterleavedMultiInputIndexedDataStream<VertexInfo> input_stream(
            input_streams);

        // Should pre-allocate, since we know the number of vertices
        VertexInfo v;
        while (input_stream.read(v)) {
            [[maybe_unused]] size_t v_id = m_mesh.get_num_vertices();

            m_mesh.add_vertex(
                {static_cast<Scalar>(v.pos[0]),
                 static_cast<Scalar>(v.pos[1]),
                 static_cast<Scalar>(v.pos[2])});

            if constexpr (OutputVertexDepth) {
                m_vertex_depth_attribute->ref(v_id) = static_cast<Scalar>(v.depth);
            }
        }
    }

protected:
    SurfaceMesh<Scalar, Index>& m_mesh;
    Attribute<Scalar>* m_vertex_depth_attribute;

    std::atomic<size_t> m_size = 0;
    struct VertexInfo
    {
        PoissonRecon::Point<ReconScalar, Dim> pos;
        ReconScalar depth;
    };
    std::vector<std::vector<std::pair<size_t, VertexInfo>>> m_vertices;
};

// A stream into which we can write the output vertices of the extracted mesh
template <typename Scalar, typename Index, typename ValueType, bool OutputVertexDepth>
struct OutputVertexStreamWithAttribute
    : public PoissonRecon::Reconstructor::
          OutputLevelSetVertexStream<ReconScalar, Dim, PoissonRecon::Point<ReconScalar>>
{
    // Construct a stream that adds vertices into the coordinates
    OutputVertexStreamWithAttribute(
        SurfaceMesh<Scalar, Index>& mesh,
        AttributeId value_attribute_id)
        : m_mesh(mesh)
        , m_value_attribute(m_mesh.template ref_attribute<ValueType>(value_attribute_id))
    {
        static_assert(!OutputVertexDepth, "[ERROR] Expected output vertex depth attribute");
        m_num_value_channels = static_cast<unsigned int>(m_value_attribute.get_num_channels());
        m_vertices.resize(ThreadPool::NumThreads());
    }

    OutputVertexStreamWithAttribute(
        SurfaceMesh<Scalar, Index>& mesh,
        AttributeId value_attribute_id,
        AttributeId vertex_depth_attribute_id)
        : m_mesh(mesh)
        , m_value_attribute(m_mesh.template ref_attribute<ValueType>(value_attribute_id))
        , m_vertex_depth_attribute(
              &m_mesh.template ref_attribute<Scalar>(vertex_depth_attribute_id))
    {
        static_assert(OutputVertexDepth, "[ERROR] Did not expect output vertex depth attribute");
        m_num_value_channels = static_cast<unsigned int>(m_value_attribute.get_num_channels());
        m_vertices.resize(ThreadPool::NumThreads());
    }

    // Override the pure abstract method from Reconstructor::OutputVertexWidthDataStream<
    // ReconScalar , Dim , Point< ReconScalar> >
    size_t write(
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const ReconScalar&,
        const PoissonRecon::Point<ReconScalar>&) override
    {
        throw std::runtime_error("Should not be called");
        return -1;
    }

    size_t write(
        unsigned int thread,
        const PoissonRecon::Point<ReconScalar, Dim>& p,
        const PoissonRecon::Point<ReconScalar, Dim>&,
        const ReconScalar& v_depth,
        const PoissonRecon::Point<ReconScalar>& data) override
    {
        size_t idx = m_size++;
        std::pair<size_t, VertexInfo> v_info;
        v_info.first = idx;
        v_info.second.pos = p;
        v_info.second.depth = v_depth;
        v_info.second.data = data;
        m_vertices[thread].push_back(v_info);
        return idx;
    }

    size_t size(void) const override { return m_size; }

    void finalize(void)
    {
        using StreamType = PoissonRecon::InputIndexedDataStream<VertexInfo>;
        using VectorStreamType = PoissonRecon::VectorBackedInputIndexedDataStream<VertexInfo>;

        std::vector<std::unique_ptr<StreamType>> input_streams_owner(m_vertices.size());
        std::vector<StreamType*> input_streams(m_vertices.size());
        for (unsigned int i = 0; i < m_vertices.size(); i++) {
            input_streams_owner[i] = std::make_unique<VectorStreamType>(m_vertices[i]);
            input_streams[i] = input_streams_owner[i].get();
        }
        PoissonRecon::InterleavedMultiInputIndexedDataStream<VertexInfo> input_stream(
            input_streams);

        // TODO: Should pre-allocate, since we know the number of vertices
        VertexInfo v;
        while (input_stream.read(v)) {
            size_t v_id = m_mesh.get_num_vertices();

            m_mesh.add_vertex(
                {static_cast<Scalar>(v.pos[0]),
                 static_cast<Scalar>(v.pos[1]),
                 static_cast<Scalar>(v.pos[2])});

            auto row = m_value_attribute.ref_row(v_id);
            for (unsigned int c = 0; c < m_num_value_channels; c++) {
                row[c] = (ValueType)v.data[c];
            }

            if constexpr (OutputVertexDepth) {
                m_vertex_depth_attribute->ref(v_id) = static_cast<Scalar>(v.depth);
            }
        }
    }

protected:
    SurfaceMesh<Scalar, Index>& m_mesh;
    Attribute<ValueType>& m_value_attribute;
    unsigned int m_num_value_channels;
    Attribute<Scalar>* m_vertex_depth_attribute;
    std::atomic<size_t> m_size = 0;

    struct VertexInfo
    {
        PoissonRecon::Point<ReconScalar, Dim> pos;
        ReconScalar depth;
        PoissonRecon::Point<ReconScalar> data;
    };
    std::vector<std::vector<std::pair<size_t, VertexInfo>>> m_vertices;
};

template <PoissonRecon::BoundaryType BoundaryType, typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> mesh_from_oriented_points_internal(
    const SurfaceMesh<Scalar, Index>& points_,
    const ReconstructionOptions& options)
{
    SurfaceMesh<Scalar, Index> points = points_; // cheap with copy-on-write

    la_runtime_assert(points.get_dimension() == 3);
    la_runtime_assert(points.get_num_facets() == 0, "Input mesh must be a point cloud!");

    // Input point coordinate attribute
    auto& input_coords = points.get_vertex_to_position();

    // Retrieve input normal attribute id
    AttributeId normal_id;
    if (options.input_normals.empty()) {
        if (auto res = find_matching_attribute(points, AttributeUsage::Normal)) {
            normal_id = res.value();
        } else {
            throw Error("Input normal attribute not found!");
        }
    } else {
        normal_id = points.get_attribute_id(options.input_normals);
    }

    // Retrieve input normal attribute buffer
    if (!points.template is_attribute_type<Scalar>(normal_id)) {
        logger().warn("Input normals do not have the same scalar type as the input points. Casting "
                      "attribute.");
        // TODO: Avoid copying the whole buffer and cast on the fly in the input stream
        normal_id = cast_attribute_in_place<Scalar>(points, normal_id);
    }
    auto& input_normals = points.template get_attribute<Scalar>(normal_id);
    la_runtime_assert(
        input_normals.get_num_channels() == 3,
        "Input normals should only have 3 channels");

    SurfaceMesh<Scalar, Index> mesh;

    // The type of reconstruction
    using ReconType = PoissonRecon::Reconstructor::Poisson;

    // Parameters for performing the reconstruction
    typename ReconType::template SolutionParameters<ReconScalar> solver_params;

    solver_params.verbose = options.verbose;
    solver_params.confidence = options.use_normal_length_as_confidence;
    solver_params.pointWeight = options.interpolation_weight;
    solver_params.samplesPerNode = options.samples_per_node;
    solver_params.depth = ensure_octree_depth(options.octree_depth, points.get_num_vertices());
    solver_params.perLevelDataScaleFactor = 32.f;

    // Parameters for extracting the level-set surface
    PoissonRecon::Reconstructor::LevelSetExtractionParameters extraction_params;
    extraction_params.linearFit = false; // Provides smoother iso-surfacing for the indicator func
    extraction_params.polygonMesh = false; // Force triangular output
    extraction_params.verbose = options.verbose;
    extraction_params.outputDensity = !options.output_vertex_depth_attribute_name.empty();

    // 1D finite-element signature
    constexpr unsigned int FEMSig =
        PoissonRecon::FEMDegreeAndBType<ReconType::DefaultFEMDegree, BoundaryType>::Signature;

    // Tensor-product finite-element signature
    using FEMSigs = PoissonRecon::IsotropicUIntPack<Dim, FEMSig>;

    if (options.interpolated_attribute_name.empty()) { // There is no attribute data

        // The type of the reconstructor
        using Implicit = typename PoissonRecon::Reconstructor::Implicit<ReconScalar, Dim, FEMSigs>;

        // The type of the solver
        using Solver = typename ReconType::Solver<ReconScalar, Dim, FEMSigs>;

        // The input data stream, generated from the points and normals
        InputPointStream<Scalar> input_points(input_coords.get_all(), input_normals.get_all());

        // Construct the implicit representation
        std::unique_ptr<Implicit> implicit(Solver::Solve(input_points, solver_params));

        // Extract the iso-surface
        if (options.output_vertex_depth_attribute_name.empty()) {
            OutputVertexStream<Scalar, Index, false> output_vertices(mesh);
            OutputTriangleStream<Scalar, Index> output_triangles(mesh);
            implicit->extractLevelSet(output_vertices, output_triangles, extraction_params);
            output_vertices.finalize();
            output_triangles.finalize();
        } else {
            mesh.template create_attribute<Scalar>(
                options.output_vertex_depth_attribute_name,
                AttributeElement::Vertex,
                AttributeUsage::Scalar);

            AttributeId vertex_depth_attribute_id =
                mesh.get_attribute_id(options.output_vertex_depth_attribute_name);

            OutputVertexStream<Scalar, Index, true> output_vertices(
                mesh,
                vertex_depth_attribute_id);
            OutputTriangleStream<Scalar, Index> output_triangles(mesh);
            implicit->extractLevelSet(output_vertices, output_triangles, extraction_params);
            output_vertices.finalize();
            output_triangles.finalize();
        }
    } else { // There is attribute data
        lagrange::AttributeId id = points.get_attribute_id(options.interpolated_attribute_name);
        internal::visit_attribute_read(points, id, [&](auto&& attribute) {
            using AttributeType = std::decay_t<decltype(attribute)>;
            using ValueType = typename AttributeType::ValueType;
            if constexpr (AttributeType::IsIndexed) {
                throw std::runtime_error("Interpolated attribute cannot be Indexed");
            } else {
                // Add the attribute to the output mesh
                mesh.template create_attribute<ValueType>(
                    options.interpolated_attribute_name,
                    AttributeElement::Vertex,
                    attribute.get_usage(), // AttributeUsage::Scalar
                    attribute.get_num_channels());

                // Get the attribute id from the input
                AttributeId attribute_id =
                    mesh.get_attribute_id(options.interpolated_attribute_name);

                // The type of the reconstructor
                using Implicit = typename PoissonRecon::Reconstructor::
                    template Implicit<ReconScalar, Dim, FEMSigs, PoissonRecon::Point<ReconScalar>>;

                // The type of the solver
                using Solver = typename ReconType::
                    Solver<ReconScalar, Dim, FEMSigs, PoissonRecon::Point<ReconScalar>>;

                // The input data stream, generated from the points and normals
                InputPointStreamWithAttribute<Scalar, ValueType> input_points(
                    input_coords.get_all(),
                    input_normals.get_all(),
                    attribute);

                // A "zero" instance for copy construction
                PoissonRecon::Point<ReconScalar> zero(attribute.get_num_channels());

                // Construct the implicit representation
                std::unique_ptr<Implicit> implicit(
                    Solver::Solve(input_points, solver_params, zero));

                // Extract the iso-surface
                if (options.output_vertex_depth_attribute_name.empty()) {
                    OutputVertexStreamWithAttribute<Scalar, Index, ValueType, false>
                        output_vertices(mesh, attribute_id);
                    OutputTriangleStream<Scalar, Index> output_triangles(mesh);
                    implicit->extractLevelSet(output_vertices, output_triangles, extraction_params);
                    output_vertices.finalize();
                    output_triangles.finalize();
                } else {
                    mesh.template create_attribute<Scalar>(
                        options.output_vertex_depth_attribute_name,
                        AttributeElement::Vertex,
                        AttributeUsage::Scalar);

                    AttributeId vertex_depth_attribute_id =
                        mesh.get_attribute_id(options.output_vertex_depth_attribute_name);

                    OutputVertexStreamWithAttribute<Scalar, Index, ValueType, true> output_vertices(
                        mesh,
                        attribute_id,
                        vertex_depth_attribute_id);
                    OutputTriangleStream<Scalar, Index> output_triangles(mesh);
                    implicit->extractLevelSet(output_vertices, output_triangles, extraction_params);
                    output_vertices.finalize();
                    output_triangles.finalize();
                }
            }
        });
    }

    return mesh;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> mesh_from_oriented_points(
    const SurfaceMesh<Scalar, Index>& points,
    const ReconstructionOptions& options)
{
    if (options.use_dirichlet_boundary) {
        return mesh_from_oriented_points_internal<PoissonRecon::BoundaryType::BOUNDARY_DIRICHLET>(
            points,
            options);
    } else {
        return mesh_from_oriented_points_internal<PoissonRecon::BoundaryType::BOUNDARY_NEUMANN>(
            points,
            options);
    }
}

#define LA_X_mesh_reconstruction(_, Scalar, Index)                 \
    template SurfaceMesh<Scalar, Index> mesh_from_oriented_points( \
        const SurfaceMesh<Scalar, Index>& points,                  \
        const ReconstructionOptions& options);
LA_SURFACE_MESH_X(mesh_reconstruction, 0)

} // namespace lagrange::poisson
