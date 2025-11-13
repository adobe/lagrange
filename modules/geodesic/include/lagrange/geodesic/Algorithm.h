/*
 * Copyright 2025 Adobe. All rights reserved.
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

namespace lagrange::geodesic {

/// The available algorithms for computing or approximating geodesic distance.
///
/// - DGPC: Melvær, Eivind Lyche, and Martin Reimers. "Geodesic polar coordinates on polygonal
/// meshes." Computer Graphics Forum. Vol. 31. No. 8. Oxford, UK: Blackwell Publishing Ltd,
/// 2012.
///
/// - MMP: Mitchell, Joseph SB, David M. Mount, and Christos H. Papadimitriou. "The discrete
/// geodesic problem." SIAM Journal on Computing 16.4 (1987): 647-668.
///
/// - HEAT: Crane, Keenan, Clarisse Weischedel, and Max Wardetzky. "Geodesics in heat: A new
/// approach to computing distance based on heat flow." ACM Transactions on Graphics (TOG) 32.5
/// (2013): 1-11.
///
/// @note MMP is often considered as as exact method, while DGPC and HEAT are approximations.
enum class Algorithm { DGPC, MMP, HEAT };

} // namespace lagrange::geodesic
