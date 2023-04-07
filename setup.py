#
# Copyright 2022 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import sys

try:
    from skbuild import setup
except ImportError:
    print(
        "The preferred way to invoke 'setup.py' is via pip, as in 'pip "
        "install .'. If you wish to run the setup script directly, you must "
        "first install the build dependencies listed in pyproject.toml!",
        file=sys.stderr,
    )
    raise

setup(
    packages=["lagrange"],
    package_dir={"lagrange": "modules/python/lagrange"},
    cmake_install_dir="modules/python/lagrange",
    include_package_data=False,
    cmake_args=[
        "-DLAGRANGE_MODULE_DECIMATION=On",
        "-DLAGRANGE_MODULE_IO=On",
        "-DLAGRANGE_MODULE_PYTHON=On",
        "-DLAGRANGE_MODULE_SCENE=On",
        "-DLAGRANGE_UNIT_TESTS=Off",
        "-DLAGRANGE_EXAMPLES=Off",
        "-DLAGRANGE_INSTALL=Off",
        "-DLAGRANGE_ASSERT_DEBUG_BREAK=Off",
        "-DTBB_PREFER_STATIC=Off",
        "-DPython_EXECUTABLE=" + sys.executable,
        # Debugging options.
        #'-DCMAKE_BUILD_TYPE=Debug',
        #'-DUSE_SANITIZER=Address',
    ],
    cmake_install_target="lagrange-python-install-runtime",
)
