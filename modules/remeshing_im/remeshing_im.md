Remeshing-Instant-Meshes Module
================================

@namespace lagrange::remeshing_im

@defgroup module-remeshing_im Remeshing-Instant-Meshes Module
@brief This module provides a headless API to the Instant Meshes library.

It exposes `lagrange::remeshing_im::remesh()`, which wraps the field-aligned
remeshing algorithm described in:

> Wenzel Jakob, Marco Tarini, Daniele Panozzo, Olga Sorkine-Hornung.
> **Instant Field-Aligned Meshes.**
> *ACM Transactions on Graphics (Proc. SIGGRAPH Asia)*, 34(6), 2015.
> DOI: [10.1145/2816795.2818078](https://doi.org/10.1145/2816795.2818078)
