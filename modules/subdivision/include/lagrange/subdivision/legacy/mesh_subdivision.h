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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/topologyDescriptor.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Core>
#include <memory>
#include <vector>


namespace lagrange {
namespace subdivision {

using SubdivisionScheme = OpenSubdiv::Sdc::SchemeType;

namespace internal {

// Vertex container implementation for OSD
template <typename MeshType>
struct OSDVertex
{
    using VertexType = typename MeshType::VertexType;
    OSDVertex() = default;
    OSDVertex(OSDVertex const& src) { m_position = src.m_position; }

    // Required interface
    void Clear(void* = 0) { m_position.setZero(); }
    void AddWithWeight(OSDVertex const& src, float weight)
    {
        m_position += (weight * src.m_position);
    }

    // Optional interface (used by Lagrange)
    void SetPosition(const VertexType& pos) { m_position = pos; }
    const VertexType& GetPosition() const { return m_position; }

private:
    VertexType m_position;
};

// Face-varying container implementation.
template <typename MeshType>
struct OSDUV
{
    using UVType = typename MeshType::UVType;
    OSDUV() {}
    OSDUV(OSDUV const& src) { m_position = src.m_position; }

    // Minimal required interface ----------------------
    void Clear(void* = 0) { m_position.setZero(); }
    void AddWithWeight(OSDUV const& src, float weight) { m_position += (weight * src.m_position); }

    // Optional interface (used by Lagrange)
    void SetPosition(const UVType& pos) { m_position = pos; }
    const UVType& GetPosition() const { return m_position; }

private:
    // Basic 'uv' layout channel
    UVType m_position;
};

} // namespace internal


// Main Entry Point

template <typename input_meshType, typename output_meshType>
std::unique_ptr<output_meshType> subdivide_mesh(
    const input_meshType& input_mesh, // input lagrange mesh
    SubdivisionScheme scheme_type, // loop? catmull-clark?
    int maxlevel, // uniform subdivision level requested
    OpenSubdiv::Sdc::Options::VtxBoundaryInterpolation vertexInterp =
        OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY,
    OpenSubdiv::Sdc::Options::FVarLinearInterpolation primvarInterp =
        OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL)
{
    static_assert(MeshTrait<input_meshType>::is_mesh(), "Input type is not Mesh");
    static_assert(
        std::is_same<typename input_meshType::Index, typename output_meshType::Index>::value,
        "Meshes must have same Index type");

    using namespace OpenSubdiv;
    using Descriptor = Far::TopologyDescriptor;
    using namespace internal;
    using OSDIndex = Vtr::Index;

    if (scheme_type == SubdivisionScheme::SCHEME_LOOP) {
        la_runtime_assert(
            input_mesh.get_vertex_per_facet() == 3,
            "Loop Subdivision only supports triangle meshes");
    }

    // Subdivision Options
    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(vertexInterp);
    options.SetFVarLinearInterpolation(primvarInterp);

    // Input info
    const auto num_vertices = input_mesh.get_num_vertices();
    const auto num_facets = input_mesh.get_num_facets();
    const auto input_vertex_per_facet = input_mesh.get_vertex_per_facet();
    const auto output_vertex_per_facet =
        output_meshType::FacetArray::ColsAtCompileTime == Eigen::Dynamic
            ? input_vertex_per_facet
            : output_meshType::FacetArray::ColsAtCompileTime;

    if (input_vertex_per_facet == 3 && output_vertex_per_facet == 4) {
        la_runtime_assert(
            maxlevel > 0,
            "Only non-zero-level subdivision is supported when the input is triangle and the "
            "output is quadrangle.");
    }

    // OpenSubdiv mesh
    Descriptor desc;
    desc.numVertices = num_vertices;
    desc.numFaces = num_facets;
    // descriptor wants an index array and an array of number of elements
    std::vector<int> verts_per_face(num_facets, input_vertex_per_facet);
    desc.numVertsPerFace = verts_per_face.data();
    la_runtime_assert(desc.numVertsPerFace != nullptr, "Invalid Vertices");
    const auto& facets = input_mesh.get_facets();
    std::vector<OSDIndex> vert_indices;
    vert_indices.resize(num_facets * input_vertex_per_facet);
    for (auto i : range(desc.numFaces)) {
        for (auto j : range(input_vertex_per_facet)) {
            vert_indices[i * input_vertex_per_facet + j] = safe_cast<OSDIndex>(facets(i, j));
        }
    }
    desc.vertIndicesPerFace = vert_indices.data();
    la_runtime_assert(desc.vertIndicesPerFace != nullptr, "Invalid Facets");

    // Create a face-varying channel descriptor
    const bool hasUvs = input_mesh.is_uv_initialized();
    std::vector<OSDIndex> uv_indices;
    const int channelUV = 0;
    const int numChannels = 1;
    Descriptor::FVarChannel channels[numChannels];
    if (hasUvs) {
        channels[channelUV].numValues = (int)input_mesh.get_uv().rows();
        const auto& inputUVIndices = input_mesh.get_uv_indices();
        uv_indices.resize(inputUVIndices.size());
        for (auto i : range(desc.numFaces)) {
            for (auto j : range(input_vertex_per_facet)) {
                uv_indices[i * input_vertex_per_facet + j] =
                    safe_cast<OSDIndex>(inputUVIndices(i, j));
            }
        }
        channels[channelUV].valueIndices = uv_indices.data();

        // Add the channel topology to the main descriptor
        desc.numFVarChannels = numChannels;
        desc.fvarChannels = channels;
    }

    // Instantiate a Far::TopologyRefiner from the descriptor
    std::unique_ptr<Far::TopologyRefiner> refiner(
        Far::TopologyRefinerFactory<Descriptor>::Create(
            desc,
            Far::TopologyRefinerFactory<Descriptor>::Options(scheme_type, options)));

    // Uniformly refine the topology up to 'maxlevel'
    {
        // note: fullTopologyInLastLevel must be true to work with face-varying data
        Far::TopologyRefiner::UniformOptions refineOptions(maxlevel);
        refineOptions.fullTopologyInLastLevel = true;
        refiner->RefineUniform(refineOptions);
    }

    // Allocate a buffer for vertex primvar data. The buffer length is set to
    // be the sum of all children vertices up to the highest level of refinement.
    std::vector<OSDVertex<output_meshType>> vbuffer(refiner->GetNumVerticesTotal());
    OSDVertex<output_meshType>* outputVerts = &vbuffer[0];
    // Initialize coarse mesh positions
    int nInputVerts = input_mesh.get_num_vertices();
    for (int i = 0; i < nInputVerts; ++i)
        outputVerts[i].SetPosition(
            input_mesh.get_vertices().row(i).template cast<typename output_meshType::Scalar>());

    OSDUV<output_meshType>* outputUvs = nullptr;
    std::vector<OSDUV<output_meshType>> fvBufferUV;
    if (hasUvs) {
        // Allocate the first channel of 'face-varying' primvars (UVs)
        fvBufferUV.resize(refiner->GetNumFVarValuesTotal(channelUV));
        outputUvs = &fvBufferUV[0];
        // initialize
        for (int i = 0; i < input_mesh.get_uv().rows(); ++i) {
            outputUvs[i].SetPosition(
                input_mesh.get_uv().row(i).template cast<typename output_meshType::Scalar>());
        }
    }

    // Interpolate both vertex and face-varying primvar data
    Far::PrimvarRefiner primvarRefiner(*refiner);
    OSDVertex<output_meshType>* srcVert = outputVerts;
    OSDUV<output_meshType>* srcFVarUV = outputUvs;
    // FVarVertexColor * srcFVarColor = fvVertsColor;
    for (int level = 1; level <= maxlevel; ++level) {
        OSDVertex<output_meshType>* dstVert =
            srcVert + refiner->GetLevel(level - 1).GetNumVertices();
        OSDUV<output_meshType>* dstFVarUV = outputUvs;
        if (hasUvs)
            dstFVarUV = srcFVarUV + refiner->GetLevel(level - 1).GetNumFVarValues(channelUV);

        primvarRefiner.Interpolate(level, srcVert, dstVert);

        if (hasUvs) primvarRefiner.InterpolateFaceVarying(level, srcFVarUV, dstFVarUV, channelUV);
        srcVert = dstVert;
        if (hasUvs) srcFVarUV = dstFVarUV;
    }

    // TODO: approximate normals

    // Generate output
    Far::TopologyLevel const& refLastLevel = refiner->GetLevel(maxlevel);
    int nOutputVerts = refLastLevel.GetNumVertices();
    int nOutputFaces = refLastLevel.GetNumFaces();
    bool triangulate = false;
    if (scheme_type == SubdivisionScheme::SCHEME_LOOP) {
        // outputs are all triangles. Not trying to quad-fy here.
        assert(output_vertex_per_facet == 3);
    } else {
        // only accepting output triangles or quads
        assert(output_vertex_per_facet == 3 || output_vertex_per_facet == 4);
        if (output_vertex_per_facet == 3) {
            // never change the topology for zero-level + triangle mesh
            if (maxlevel > 0 || input_vertex_per_facet > 3) {
                triangulate = true;
                nOutputFaces *= 2;
            }
        }
    }

    // copy verts
    int firstOfLastVerts = refiner->GetNumVerticesTotal() - nOutputVerts;
    typename output_meshType::VertexArray V;
    V.resize(nOutputVerts, 3);
    for (int iVert = 0; iVert < nOutputVerts; ++iVert) {
        typename output_meshType::VertexType pos =
            outputVerts[firstOfLastVerts + iVert].GetPosition();
        V.row(iVert) = pos;
    }

    // copy faces
    typename output_meshType::FacetArray F;
    F.resize(nOutputFaces, F.cols());
    for (int iFace = 0; iFace < refLastLevel.GetNumFaces(); ++iFace) {
        Far::ConstIndexArray facetIndices = refLastLevel.GetFaceVertices(iFace);
        if (triangulate) {
            F(iFace * 2, 0) = facetIndices[0];
            F(iFace * 2, 1) = facetIndices[1];
            F(iFace * 2, 2) = facetIndices[2];
            F(iFace * 2 + 1, 0) = facetIndices[0];
            F(iFace * 2 + 1, 1) = facetIndices[2];
            F(iFace * 2 + 1, 2) = facetIndices[3];
        } else {
            for (int i = 0; i < facetIndices.size(); ++i) {
                F(iFace, i) = facetIndices[i];
            }
        }
    }

    std::unique_ptr<output_meshType> output_mesh = create_mesh(std::move(V), std::move(F));

    // copy uvs and indices
    if (hasUvs) {
        int nOutputUvs = refLastLevel.GetNumFVarValues(channelUV);
        int firstOfLastUvs = refiner->GetNumFVarValuesTotal(channelUV) - nOutputUvs;
        typename output_meshType::UVArray UV;
        typename output_meshType::FacetArray UVF;
        UV.resize(nOutputUvs, 2);
        UVF.resize(nOutputFaces, output_vertex_per_facet);
        for (int iFVVert = 0; iFVVert < nOutputUvs; ++iFVVert) {
            typename output_meshType::UVType uv = outputUvs[firstOfLastUvs + iFVVert].GetPosition();
            UV.row(iFVVert) = uv;
        }
        for (int iFace = 0; iFace < refLastLevel.GetNumFaces(); ++iFace) {
            Far::ConstIndexArray uvIndices = refLastLevel.GetFaceFVarValues(iFace, channelUV);
            if (triangulate) {
                UVF(iFace * 2, 0) = uvIndices[0];
                UVF(iFace * 2, 1) = uvIndices[1];
                UVF(iFace * 2, 2) = uvIndices[2];
                UVF(iFace * 2 + 1, 0) = uvIndices[0];
                UVF(iFace * 2 + 1, 1) = uvIndices[2];
                UVF(iFace * 2 + 1, 2) = uvIndices[3];
            } else {
                for (int i = 0; i < uvIndices.size(); ++i) {
                    UVF(iFace, i) = uvIndices[i];
                }
            }
        }
        output_mesh->initialize_uv(UV, UVF);
    }

    // TODO: add colors too...
    return output_mesh;
}
} // namespace subdivision
} // namespace lagrange
