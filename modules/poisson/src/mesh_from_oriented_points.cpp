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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/Attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>
#include <lagrange/internal/visit_attribute.h>

#include <PreProcessor.h>
#include <Reconstructors.h>

namespace lagrange::poisson {
namespace {
using ReconScalar = float;
const unsigned int Dim = 3;

template <typename PType, typename NType>
struct InputPointStream : public Reconstructor::InputSampleStream<ReconScalar, Dim>
{
    // Constructs a stream that contains the specified number of samples
    InputPointStream(const PType& P, const NType& N)
        : _P(P)
        , _N(N)
        , _current(0)
    {
        la_runtime_assert(_P.rows() == _N.rows(), "Number of normals and points don't match");
        la_runtime_assert(_P.cols() == 3, "Points should be three-dimensional");
        la_runtime_assert(_N.cols() == 3, "Normals should be three-dimensional");
    }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    void reset(void) { _current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    bool base_read(Point<ReconScalar, Dim>& p, Point<ReconScalar, Dim>& n)
    {
        if (_current < _P.rows()) {
            for (unsigned int d = 0; d < Dim; d++)
                p[d] = (ReconScalar)_P(_current, d), n[d] = (ReconScalar)_N(_current, d);
            _current++;
            return true;
        } else
            return false;
    }

protected:
    const PType& _P;
    const NType& _N;
    unsigned int _current;
};

// template <typename PType, typename NType, typename Scalar, typename ValueType = Scalar>
template <typename PType, typename NType, typename ValueType>
struct InputPointStreamWithAttribute
    : public Reconstructor::InputSampleWithDataStream<ReconScalar, Dim, Point<ReconScalar>>
{
    // Constructs a stream that contains the specified number of samples
    InputPointStreamWithAttribute(
        const PType& P,
        const NType& N,
        const Attribute<ValueType>& attribute)
        : _P(P)
        , _N(N)
        , _attribute(attribute)
        , _current(0)
        , _channels((unsigned int)_attribute.get_num_channels())
        , Reconstructor::InputSampleWithDataStream<ReconScalar, Dim, Point<ReconScalar>>(
              Point<ReconScalar>(attribute.get_num_channels()))
    {
        la_runtime_assert(_P.rows() == _N.rows(), "Number of normals and points don't match");
        la_runtime_assert(_P.cols() == 3, "Points should be three-dimensional");
        la_runtime_assert(_N.cols() == 3, "Normals should be three-dimensional");
        la_runtime_assert(
            _P.rows() == _attribute.get_num_elements(),
            "Number of attribute elements doesn't match number of vertices");
    }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    void reset(void) { _current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    bool base_read(Point<ReconScalar, Dim>& p, Point<ReconScalar, Dim>& n, Point<ReconScalar>& data)
    {
        if (_current < _P.rows()) {
            // Copy the positions and the normals
            for (unsigned int d = 0; d < Dim; d++) {
                p[d] = (ReconScalar)_P(_current, d);
                n[d] = (ReconScalar)_N(_current, d);
            }

            // Copy the attribute data
            auto row = _attribute.get_row(_current);
            for (unsigned int c = 0; c < _channels; c++) data[c] = (ReconScalar)row[c];

            _current++;
            return true;
        } else {
            return false;
        }
    }

protected:
    const PType& _P;
    const NType& _N;
    const Attribute<ValueType>& _attribute;
    unsigned int _channels;
    unsigned int _current;
};

// A stream into which we can write polygons of the form std::vector< node_index_type >
template <typename Scalar, typename Index>
struct OutputTriangleStream : public Reconstructor::OutputFaceStream<2>
{
    // Construct a stream that adds polygons to the vector of polygons
    OutputTriangleStream(SurfaceMesh<Scalar, Index>& mesh)
        : _mesh(mesh)
    {}


    // Override the pure abstract method from OutputPolygonStream
    void base_write(const std::vector<node_index_type>& polygon)
    {
        la_runtime_assert(polygon.size() == Dim, "Expected triangular face");
        _mesh.add_triangle((Index)polygon[0], (Index)polygon[1], (Index)polygon[2]);
    }

protected:
    SurfaceMesh<Scalar, Index>& _mesh;
};

// A stream into which we can write the output vertices of the extracted mesh
template <typename Scalar, typename Index>
struct OutputVertexStream : public Reconstructor::OutputVertexStream<ReconScalar, Dim>
{
    // Construct a stream that adds vertices into the coordinates
    OutputVertexStream(SurfaceMesh<Scalar, Index>& mesh)
        : _mesh(mesh)
    {}

    // Override the pure abstract method from Reconstructor::OutputVertexStream< Scalar , Dim >
    void base_write(Point<ReconScalar, Dim> p, Point<ReconScalar, Dim>, ReconScalar)
    {
        _mesh.add_vertex({(Scalar)p[0], (Scalar)p[1], (Scalar)p[2]});
    }

protected:
    SurfaceMesh<Scalar, Index>& _mesh;
};

// A stream into which we can write the output vertices of the extracted mesh
template <typename Scalar, typename Index, typename ValueType>
struct OutputVertexStreamWithAttribute
    : public Reconstructor::OutputVertexWithDataStream<ReconScalar, Dim, Point<ReconScalar>>
{
    // Construct a stream that adds vertices into the coordinates
    OutputVertexStreamWithAttribute(SurfaceMesh<Scalar, Index>& mesh, AttributeId attribute_id)
        : _mesh(mesh)
        , _attribute(_mesh.template ref_attribute<ValueType>(attribute_id))
    {
        _channels = (unsigned int)_attribute.get_num_channels();
    }

    // Override the pure abstract method from Reconstructor::OutputVertexWidthDataStream<
    // ReconScalar , Dim , Point< ReconScalar> >
    void base_write(
        Point<ReconScalar, Dim> p,
        Point<ReconScalar, Dim>,
        ReconScalar,
        Point<ReconScalar> data)
    {
        size_t vId = _mesh.get_num_vertices();
        _mesh.add_vertex({(Scalar)p[0], (Scalar)p[1], (Scalar)p[2]});
        auto row = _attribute.ref_row(vId);
        for (unsigned int c = 0; c < _channels; c++) row[c] = (ValueType)data[c];
    }

protected:
    SurfaceMesh<Scalar, Index>& _mesh;
    Attribute<ValueType>& _attribute;
    unsigned int _channels;
};
} // namespace


template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> mesh_from_oriented_points(
    const SurfaceMesh<Scalar, Index>& points,
    const ReconstructionOptions& options)
{
    la_runtime_assert(points.get_dimension() == 3);
    la_runtime_assert(points.get_num_facets() == 0, "Input mesh must be a point cloud!");

    // Eigen-like matrix of points coordinates
    auto P = vertex_view(points);

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

    span<const Scalar> attribute_data;
    if (!options.attribute_name.empty()) {
        attribute_data = points.template get_attribute<Scalar>(options.attribute_name).get_all();
        const Scalar* foo = attribute_data.data(); // A raw pointer to the data
        size_t sz = attribute_data.size();
    }

    // Eigen-like matrix of normal vectors
    auto N = attribute_matrix_view<Scalar>(points, normal_id);

    // TODO: Fill in the implementation here. Reconstruct mesh from (P, N) matrices.
    SurfaceMesh<Scalar, Index> mesh;

    // The type of reconstruction
    using ReconType = Reconstructor::Poisson;

    // Finite-elements signature
    static const unsigned int FEMSig =
        FEMDegreeAndBType<ReconType::DefaultFEMDegree, ReconType::DefaultFEMBoundary>::Signature;

    // Parameters for performing the reconstruction
    typename ReconType::template SolutionParameters<ReconScalar> solverParams;

    solverParams.verbose = options.show_logging_output;
    solverParams.confidence = (ReconScalar)(options.use_normal_length_as_confidence ? 1 : 0);
    solverParams.pointWeight = options.interpolation_weight;
    solverParams.targetValue = 0.5f;
    if (!options.octree_depth) {
        solverParams.depth = (unsigned int)ceil(log(points.get_num_vertices()) / log(4.));
        std::cout << "Setting depth from point count: " << points.get_num_vertices() << " -> "
                  << solverParams.depth << std::endl;
    } else
        solverParams.depth = options.octree_depth;
    // Parameters for exracting the level-set surface
    Reconstructor::LevelSetExtractionParameters extractionParams;
    extractionParams.linearFit =
        false; // Provides smoother iso-surfacing for the indicator function
    extractionParams.polygonMesh = false; // Force triangular output
    extractionParams.verbose = options.show_logging_output;


    if (options.attribute_name.empty()) { // There is no attribute data

        // The type of the reconstructor
        using Implicit = typename ReconType::template Implicit<ReconScalar, Dim, FEMSig>;

        // The input data stream, generated from the points and normals
        InputPointStream<decltype(P), decltype(N)> inputPoints(P, N);

        // Construct the implicit representation
        Implicit implicit(inputPoints, solverParams);

        // Extract the iso-surface
        OutputVertexStream<Scalar, Index> outputVertices(mesh);
        OutputTriangleStream<Scalar, Index> outputTriangles(mesh);
        implicit.extractLevelSet(outputVertices, outputTriangles, extractionParams);

    } else { // There is attribute data
#if 0
internal::visit_attribute_read(mesh, id, [&](auto &&attr) {
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;

    ...
});
#endif
        lagrange::AttributeId id = points.get_attribute_id(options.attribute_name);
        internal::visit_attribute_read(points, id, [&](auto&& attribute) {
            using AttributeType = std::decay_t<decltype(attribute)>;
            using ValueType = typename AttributeType::ValueType;
            // Get the attribute from the input
            //            const Attribute<Scalar>& attribute =
            //                points.template get_attribute<Scalar>(options.attribute_name);

            // Add the attribute to the output mesh
            mesh.template create_attribute<ValueType>(
                options.attribute_name,
                AttributeElement::Vertex,
                attribute.get_usage(),
                attribute.get_num_channels());

            // Get the attribute id from the input
            AttributeId attribute_id = mesh.get_attribute_id(options.attribute_name);

            // The type of the reconstructor
            using Implicit =
                typename ReconType::template Implicit<ReconScalar, Dim, FEMSig, Point<ReconScalar>>;

            // The input data stream, generated from the points and normals
            InputPointStreamWithAttribute<decltype(P), decltype(N), ValueType> inputPoints(
                P,
                N,
                attribute);

            // Construct the implicit representation
            Implicit implicit(inputPoints, solverParams);

            // Scale the color information to give extrapolation preference to data at finer depths
            implicit.weightAuxDataByDepth((ReconScalar)32.);

            // Extract the iso-surface
            OutputVertexStreamWithAttribute<Scalar, Index, ValueType> outputVertices(
                mesh,
                attribute_id);
            OutputTriangleStream<Scalar, Index> outputTriangles(mesh);
            implicit.extractLevelSet(outputVertices, outputTriangles, extractionParams);
        });
    }

    (void)P;
    (void)N;

    return mesh;
}

#define LA_X_mesh_reconstruction(_, Scalar, Index)                 \
    template SurfaceMesh<Scalar, Index> mesh_from_oriented_points( \
        const SurfaceMesh<Scalar, Index>& points,                  \
        const ReconstructionOptions& options);
LA_SURFACE_MESH_X(mesh_reconstruction, 0)
} // namespace lagrange::poisson
