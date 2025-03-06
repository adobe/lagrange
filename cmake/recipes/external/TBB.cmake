#
# Copyright 2019 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

####################################################################################################
# IMPORTANT
#
# This file defines a single ALIAS target `TBB::tbb`.
#
# Depending on the option TBB_PREFER_STATIC, this alias may point to either the dynamic version
# of TBB, or the static version. The official recommendation is to use the dynamic library:
#
# https://www.threadingbuildingblocks.org/faq/there-version-tbb-provides-statically-linked-libraries
# https://stackoverflow.com/questions/638278/how-to-statically-link-to-tbb
#
# For now we do not have a proper CMake workflow to deal with DLLs, so we default to tbb_static
####################################################################################################

if(TARGET TBB::tbb)
    return()
endif()

include(onetbb)
