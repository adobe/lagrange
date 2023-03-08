/*
 * Copyright 2022 Adobe. All rights reserved.
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

#ifdef LAGRANGE_WITH_MIKKTSPACE

    #include <lagrange/SurfaceMesh.h>
    #include <lagrange/internal/find_attribute_utils.h>
    #include <lagrange/utils/assert.h>
    #include <lagrange/utils/safe_cast.h>
    #include <lagrange/views.h>

    #include <mikktspace.h>

namespace lagrange {

namespace {

///
/// Computes the tangent/bitangent vectors using mikktspace. Note that the generated attributes will
/// always be a corner attribute, regardless of the input option's output_element_type. Furthermore,
/// output corners that correspond to the same tangent frame will share the same value.
///
/// @param[in]  mesh               The input mesh.
/// @param[in]  options            Optional arguments to control tangent/bitangent generation.
///
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
/// @tparam     NVPF               Number of vertex per facet.
/// @tparam     DIM                Mesh dimension.
/// @tparam     UV_DIM             Mesh UV dimension.
///
/// @return     A struct containing the id of the generated tangent/bitangent attributes.
///
template <typename Scalar, typename Index, int NVPF = 3, int DIM = 3, int UV_DIM = 2>
lagrange::TangentBitangentResult compute_tangent_bitangent_mikktspace(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    lagrange::TangentBitangentOptions options = {})
{
    la_runtime_assert(mesh.get_dimension() == DIM, "Mesh must be 3D");
    la_runtime_assert(mesh.is_triangle_mesh(), "Input must be a triangle mesh");
    static_assert(NVPF == 3);
    static_assert(DIM == 3);
    static_assert(UV_DIM == 2);

    lagrange::TangentBitangentResult result;

    auto uv_id = internal::find_matching_attribute<Scalar>(
        mesh,
        options.uv_attribute_name,
        AttributeElement::Indexed,
        AttributeUsage::UV,
        UV_DIM);
    la_runtime_assert(uv_id != invalid_attribute_id(), "Mesh must have indexed UVs");
    auto normal_id = internal::find_matching_attribute<Scalar>(
        mesh,
        options.normal_attribute_name,
        AttributeElement::Indexed,
        AttributeUsage::Normal,
        DIM);
    la_runtime_assert(normal_id != invalid_attribute_id(), "Mesh must have indexed normals");

    const size_t num_channels = (options.pad_with_sign ? 4 : 3);

    result.tangent_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.tangent_attribute_name,
        AttributeElement::Corner,
        AttributeUsage::Tangent,
        num_channels,
        internal::ResetToDefault::No);

    result.bitangent_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.bitangent_attribute_name,
        AttributeElement::Corner,
        AttributeUsage::Bitangent,
        num_channels,
        internal::ResetToDefault::No);

    struct LocalData
    {
        int num_facets;
        int num_channels;

        span<const Scalar> position_values;
        span<const Index> position_indices;

        span<const Scalar> normal_values;
        span<const Index> normal_indices;

        span<const Scalar> uv_values;
        span<const Index> uv_indices;

        span<Scalar> tangents;
        span<Scalar> bitangents;
    };

    LocalData data;
    data.num_facets = lagrange::safe_cast<int>(mesh.get_num_facets());
    data.num_channels = static_cast<int>(num_channels);
    data.position_values = mesh.get_vertex_to_position().get_all();
    data.position_indices = mesh.get_corner_to_vertex().get_all();
    data.normal_values = mesh.template get_indexed_attribute<Scalar>(normal_id).values().get_all();
    data.normal_indices =
        mesh.template get_indexed_attribute<Scalar>(normal_id).indices().get_all();
    data.uv_values = mesh.template get_indexed_attribute<Scalar>(uv_id).values().get_all();
    data.uv_indices = mesh.template get_indexed_attribute<Scalar>(uv_id).indices().get_all();
    data.tangents = mesh.template ref_attribute<Scalar>(result.tangent_id).ref_all();
    data.bitangents = mesh.template ref_attribute<Scalar>(result.bitangent_id).ref_all();

    SMikkTSpaceInterface mk_interface;
    mk_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) {
        auto _data = reinterpret_cast<const LocalData*>(pContext->m_pUserData);
        return _data->num_facets;
    };
    mk_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext* pContext, const int iFace) {
        (void)pContext;
        (void)iFace;
        return NVPF;
    };
    mk_interface.m_getPosition =
        [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
            auto _data = reinterpret_cast<const LocalData*>(pContext->m_pUserData);
            const Index v = _data->position_indices[iFace * NVPF + iVert];
            auto pos = _data->position_values.subspan(v * DIM, DIM);
            fvPosOut[0] = static_cast<float>(pos[0]);
            fvPosOut[1] = static_cast<float>(pos[1]);
            fvPosOut[2] = static_cast<float>(pos[2]);
        };
    mk_interface.m_getNormal = [](const SMikkTSpaceContext* pContext,
                                  float fvNormOut[],
                                  const int iFace,
                                  const int iVert) {
        auto _data = reinterpret_cast<const LocalData*>(pContext->m_pUserData);
        const Index v = _data->normal_indices[iFace * NVPF + iVert];
        auto nrm = _data->normal_values.subspan(v * DIM, DIM);
        fvNormOut[0] = static_cast<float>(nrm[0]);
        fvNormOut[1] = static_cast<float>(nrm[1]);
        fvNormOut[2] = static_cast<float>(nrm[2]);
    };
    mk_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext,
                                    float fvTexcOut[],
                                    const int iFace,
                                    const int iVert) {
        auto _data = reinterpret_cast<const LocalData*>(pContext->m_pUserData);
        const Index v = _data->uv_indices[iFace * NVPF + iVert];
        auto uv = _data->uv_values.subspan(v * UV_DIM, UV_DIM);
        fvTexcOut[0] = static_cast<float>(uv[0]);
        fvTexcOut[1] = static_cast<float>(uv[1]);
    };
    mk_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext,
                                       const float fvTangent[],
                                       const float fSign,
                                       const int iFace,
                                       const int iVert) {
        auto _data = reinterpret_cast<LocalData*>(pContext->m_pUserData);
        auto tangent =
            _data->tangents.subspan((iFace * NVPF + iVert) * _data->num_channels, _data->num_channels);
        tangent[0] = static_cast<Scalar>(fvTangent[0]);
        tangent[1] = static_cast<Scalar>(fvTangent[1]);
        tangent[2] = static_cast<Scalar>(fvTangent[2]);
        if (tangent.size() == 4) {
            tangent[3] = static_cast<Scalar>(fSign);
        }
    };
    mk_interface.m_setTSpace = [](const SMikkTSpaceContext* pContext,
                                  const float fvTangent[],
                                  const float fvBiTangent[],
                                  const float fMagS,
                                  const float fMagT,
                                  const tbool bIsOrientationPreserving,
                                  const int iFace,
                                  const int iVert) {
        (void)fMagS;
        (void)fMagT;
        auto _data = reinterpret_cast<LocalData*>(pContext->m_pUserData);
        const float fSign = bIsOrientationPreserving ? 1.0f : (-1.0f);

        auto tangent =
            _data->tangents.subspan((iFace * NVPF + iVert) * _data->num_channels, _data->num_channels);
        tangent[0] = static_cast<Scalar>(fvTangent[0]);
        tangent[1] = static_cast<Scalar>(fvTangent[1]);
        tangent[2] = static_cast<Scalar>(fvTangent[2]);
        if (tangent.size() == 4) {
            tangent[3] = static_cast<Scalar>(fSign);
        }

        auto bitangent = _data->bitangents.subspan(
            (iFace * NVPF + iVert) * _data->num_channels,
            _data->num_channels);
        bitangent[0] = static_cast<Scalar>(fvBiTangent[0]);
        bitangent[1] = static_cast<Scalar>(fvBiTangent[1]);
        bitangent[2] = static_cast<Scalar>(fvBiTangent[2]);
        if (bitangent.size() == 4) {
            bitangent[3] = static_cast<Scalar>(fSign);
        }
    };

    SMikkTSpaceContext context;
    context.m_pInterface = &mk_interface;
    context.m_pUserData = reinterpret_cast<void*>(&data);

    lagrange::logger().debug("run mikktspace");
    genTangSpaceDefault(&context);
    lagrange::logger().debug("done");

    return result;
}

} // namespace

} // namespace lagrange

#endif
