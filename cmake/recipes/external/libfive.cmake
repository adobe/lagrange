#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
if(TARGET libfive::libfive)
    return()
endif()

message(STATUS "Third-party (external): creating target 'libfive::libfive'")

set(LIBFIVE_HASH 248c15c57abd2b1b9ea0e05d0a40f579d225f00f)

include(eigen)
include(boost)

include(CPM)
CPMAddPackage(
    NAME libfive
    GITHUB_REPOSITORY libfive/libfive
    GIT_TAG ${LIBFIVE_HASH}
    DOWNLOAD_ONLY ON
)

add_library(libfive STATIC)
target_sources(libfive
    PRIVATE
    ${libfive_SOURCE_DIR}/libfive/src/eval/base.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/deck.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/eval_interval.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/eval_jacobian.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/eval_array.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/eval_deriv_array.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/eval_feature.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/tape.cpp
    ${libfive_SOURCE_DIR}/libfive/src/eval/feature.cpp

    # ${libfive_SOURCE_DIR}/libfive/src/render/discrete/heightmap.cpp FIXME need png lib
    # ${libfive_SOURCE_DIR}/libfive/src/render/discrete/voxels.cpp

    ${libfive_SOURCE_DIR}/libfive/src/render/brep/contours.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/edge_tables.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/manifold_tables.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/mesh.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/neighbor_tables.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/progress.cpp

    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/marching.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_contourer.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_mesher.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_neighbors2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_neighbors3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_worker_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_worker_pool3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_tree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_tree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_xtree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_xtree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_object_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/dc/dc_object_pool3.cpp

    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_debug.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_worker_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_worker_pool3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_neighbors2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_neighbors3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_tree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_tree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_xtree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_xtree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_object_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_object_pool3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/hybrid/hybrid_mesher.cpp

    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_debug.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_neighbors2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_neighbors3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_worker_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_worker_pool3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_tree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_tree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_xtree2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_xtree3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_object_pool2.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_object_pool3.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/simplex/simplex_mesher.cpp

    ${libfive_SOURCE_DIR}/libfive/src/render/brep/vol/vol_neighbors.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/vol/vol_object_pool.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/vol/vol_tree.cpp
    ${libfive_SOURCE_DIR}/libfive/src/render/brep/vol/vol_worker_pool.cpp

    ${libfive_SOURCE_DIR}/libfive/src/solve/solver.cpp

    ${libfive_SOURCE_DIR}/libfive/src/tree/opcode.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/archive.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/data.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/deserializer.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/serializer.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/tree.cpp
    ${libfive_SOURCE_DIR}/libfive/src/tree/operations.cpp

    ${libfive_SOURCE_DIR}/libfive/src/oracle/oracle_clause.cpp
    ${libfive_SOURCE_DIR}/libfive/src/oracle/transformed_oracle.cpp
    ${libfive_SOURCE_DIR}/libfive/src/oracle/transformed_oracle_clause.cpp

    ${libfive_SOURCE_DIR}/libfive/src/libfive.cpp
)
target_include_directories(libfive
    PUBLIC
    ${libfive_SOURCE_DIR}/libfive/include
)
target_link_libraries(libfive
    PUBLIC
    Eigen3::Eigen
    Boost::algorithm
    Boost::bimap
    Boost::container
    Boost::functional
    Boost::lockfree
    Boost::numeric_interval
)
target_compile_definitions(libfive
    PRIVATE
    GIT_TAG="${LIBFIVE_HASH}"
    GIT_REV="${LIBFIVE_HASH}"
    GIT_BRANCH="${LIBFIVE_HASH}"
    PUBLIC
    _USE_MATH_DEFINES
    NOMINMAX
)
if(MSVC)
    target_compile_options(libfive
        PRIVATE
        /bigobj
    )
endif()
add_library(libfive::libfive ALIAS libfive)
set_target_properties(libfive PROPERTIES FOLDER third_party/libfive)
