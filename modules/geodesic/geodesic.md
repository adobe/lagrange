Geodesic Module
============

@namespace lagrange::geodesic

@defgroup module-geodesic Geodesic Module
@brief Geodesic distance computation on meshes.

The base `lagrange::geodesic` target provides the generic and DGPC engines without a
geometry-central dependency. The heat-method, MMP, and flip engines are provided by the optional
`geometrycentral` component.

@code{.cmake}
lagrange_include_module(geodesic COMPONENTS geometrycentral)
target_link_libraries(my_target PRIVATE lagrange::geodesic::geometrycentral)
@endcode

Including `GeodesicEngineHeat.h`, `GeodesicEngineMMP.h`, or `GeodesicEngineFlip.h` without linking
the component produces a compile-time error that names the required target.

- Common interface: [GeodesicEngine](@ref lagrange::geodesic::GeodesicEngine).
- Single-source and point-to-point geodesic distances: [SingleSourceGeodesicOptions](@ref lagrange::geodesic::SingleSourceGeodesicOptions), [PointToPointGeodesicOptions](@ref lagrange::geodesic::PointToPointGeodesicOptions).
- Surface path extraction: [PointToPointGeodesicPathOptions](@ref lagrange::geodesic::PointToPointGeodesicPathOptions).
- Algorithms: [GeodesicEngineDGPC](@ref lagrange::geodesic::GeodesicEngineDGPC), [GeodesicEngineHeat](@ref lagrange::geodesic::GeodesicEngineHeat), [GeodesicEngineMMP](@ref lagrange::geodesic::GeodesicEngineMMP), [GeodesicEngineFlip](@ref lagrange::geodesic::GeodesicEngineFlip).
