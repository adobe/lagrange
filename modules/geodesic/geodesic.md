Geodesic Module
============

@namespace lagrange::geodesic

@defgroup module-geodesic Geodesic Module
@brief Geodesic distance computation on meshes.

The base `lagrange::geodesic` target provides the generic and DGPC engines without a
geometry-central dependency. The heat-method and MMP engines are provided by the optional
`geometrycentral` component.

@code{.cmake}
lagrange_include_module(geodesic COMPONENTS geometrycentral)
target_link_libraries(my_target PRIVATE lagrange::geodesic::geometrycentral)
@endcode

Including `GeodesicEngineHeat.h` or `GeodesicEngineMMP.h` without linking the component produces a
compile-time error that names the required target.
