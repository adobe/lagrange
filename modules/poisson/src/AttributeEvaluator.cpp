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
#include <lagrange/poisson/AttributeEvaluator.h>

#include "octree_depth.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
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
#include <Extrapolator.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <tbb/enumerable_thread_specific.h>

#include <variant>

namespace lagrange::poisson {

namespace {

using ReconScalar = float;
constexpr unsigned int Dim = 3;

// template <typename ValueType>
// using VectorX = Eigen::VectorX<ValueType>;
template <typename ValueType>
using VectorX = PoissonRecon::Point<ValueType>;

template <typename MeshScalar, typename ValueType>
struct ColoredPointStreamWithAttribute
    : public PoissonRecon::Reconstructor::InputSampleStream<ReconScalar, Dim, VectorX<ValueType>>
{
    // Constructs a stream that contains the specified number of samples
    ColoredPointStreamWithAttribute(span<const MeshScalar> P, const Attribute<ValueType>& attribute)
        : m_points(P)
        , m_attribute(attribute)
        , m_num_channels(static_cast<unsigned int>(m_attribute.get_num_channels()))
        , m_current(0)
    {
        la_runtime_assert(
            m_points.size() / Dim == m_attribute.get_num_elements(),
            "Number of attribute elements doesn't match number of vertices");
    }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    void reset() override { m_current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Scalar , Dim >
    bool read(PoissonRecon::Point<ReconScalar, Dim>& p, VectorX<ValueType>& data) override
    {
        if (m_current * Dim < m_points.size()) {
            // Copy the positions
            for (unsigned int d = 0; d < Dim; d++) {
                p[d] = static_cast<ReconScalar>(m_points[m_current * Dim + d]);
            }

            // Copy the attribute data
            auto row = m_attribute.get_row(m_current);
            for (unsigned int c = 0; c < m_num_channels; c++) {
                data[c] = row[c];
            }

            m_current++;
            return true;
        } else {
            return false;
        }
    }

    bool read(unsigned int, PoissonRecon::Point<ReconScalar, Dim>& p, VectorX<ValueType>& data)
        override
    {
        return read(p, data);
    }

protected:
    span<const MeshScalar> m_points;
    const Attribute<ValueType>& m_attribute;
    unsigned int m_num_channels;
    unsigned int m_current;
};

template <typename ValueType>
using Extrapolator = PoissonRecon::Extrapolator::Implicit<ReconScalar, Dim, VectorX<ValueType>>;

} // namespace

struct AttributeEvaluator::Impl
{
    template <typename ValueType>
    struct Derived;

    Impl() = default;
    virtual ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;
};

template <typename ValueType>
struct AttributeEvaluator::Impl::Derived : public AttributeEvaluator::Impl
{
    template <typename PointStream, typename Params, typename AuxData>
    Derived(PointStream&& points, Params&& params, AuxData&& aux_)
        : extrapolator(
              std::forward<PointStream>(points),
              std::forward<Params>(params),
              std::forward<AuxData>(aux_))
        , aux(aux_)
    {}

    Extrapolator<ValueType> extrapolator;

    tbb::enumerable_thread_specific<VectorX<ValueType>> aux;
};

template <typename Scalar, typename Index>
AttributeEvaluator::AttributeEvaluator(
    const SurfaceMesh<Scalar, Index>& points,
    const EvaluatorOptions& options)
{
    la_runtime_assert(points.get_dimension() == 3);
    la_runtime_assert(points.get_num_facets() == 0, "Input mesh must be a point cloud!");

    // Input point coordinate attribute
    auto& input_coords = points.get_vertex_to_position();

    lagrange::AttributeId id = points.get_attribute_id(options.interpolated_attribute_name);
    internal::visit_attribute_read(points, id, [&](auto&& attribute) {
        using AttributeType = std::decay_t<decltype(attribute)>;
        using ValueType = typename AttributeType::ValueType;
        if constexpr (AttributeType::IsIndexed) {
            throw std::runtime_error("Interpolated attribute cannot be Indexed");
        } else if constexpr (std::is_integral_v<ValueType>) {
            throw std::runtime_error("Interpolated attribute ValueType cannot be integral");
        } else {
            // Parameters for performing the extrapolation
            typename PoissonRecon::Extrapolator::Implicit<ReconScalar, Dim, VectorX<ValueType>>::
                Parameters e_params;
            e_params.verbose = options.verbose;
            e_params.depth = ensure_octree_depth(options.octree_depth, points.get_num_vertices());

            VectorX<ValueType> zero(attribute.get_num_channels());

            // The input data stream, generated from the points and attribute
            ColoredPointStreamWithAttribute<Scalar, ValueType> input_points(
                input_coords.get_all(),
                attribute);

            // The extrapolated attribute field
            m_impl =
                std::make_unique<Impl::Derived<ValueType>>(std::ref(input_points), e_params, zero);
        }
    });
}

AttributeEvaluator::~AttributeEvaluator() = default;

template <typename Scalar, typename ValueType>
void AttributeEvaluator::eval(span<const Scalar> pos, span<ValueType> out) const
{
    int thread_index = tbb::this_task_arena::current_thread_index();
    int max_threads = tbb::this_task_arena::max_concurrency();
    la_runtime_assert(thread_index < max_threads);

    PoissonRecon::Point<ReconScalar, Dim> p(
        static_cast<ReconScalar>(pos[0]),
        static_cast<ReconScalar>(pos[1]),
        static_cast<ReconScalar>(pos[2]));

    Impl::Derived<ValueType>& impl = static_cast<Impl::Derived<ValueType>&>(*m_impl);
    auto& aux = impl.aux.local();
    impl.extrapolator.evaluate(thread_index, p, aux);
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = aux[i];
    }
}

#define LA_X_attribute_evaluator(_, Scalar, Index)   \
    template AttributeEvaluator::AttributeEvaluator( \
        const SurfaceMesh<Scalar, Index>& points,    \
        const EvaluatorOptions& options);
LA_SURFACE_MESH_X(attribute_evaluator, 0)

#define LA_X_eval_func(ValueType, Scalar) \
    template void AttributeEvaluator::eval(span<const Scalar> pos, span<ValueType> out) const;
#define LA_X_eval_aux(_, ValueType) LA_SURFACE_MESH_SCALAR_X(eval_func, ValueType)
LA_ATTRIBUTE_SCALAR_X(eval_aux, 0)

} // namespace lagrange::poisson
