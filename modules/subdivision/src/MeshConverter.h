#pragma once

#include <lagrange/subdivision/mesh_subdivision.h>

namespace lagrange::subdivision {

// Proxy type to forward both mesh + user options to TopologyRefinerFactory
template <typename SurfaceMeshT>
struct MeshConverter
{
    using SurfaceMeshType = SurfaceMeshT;

    const SurfaceMeshType& mesh;
    const SubdivisionOptions& options;
    const std::vector<AttributeId>& face_varying_attributes;
};

} // namespace lagrange::subdivision
