#
# Copyright 2021 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#

# To compute the md5 checksums for each lib, use the following bash script (replace the target version number):
# for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-md5") <(conda search --override-channel --channel intel $f=2021.3.0 --platform $os-64 -i | grep md5 | cut -d : -f 2); done; done
set(mkl-linux-64-md5 2501643729c00b24fddb9530b339aea7)
set(mkl-osx-64-md5 d6129ae9dfba58671667a65c160d0776)
set(mkl-win-64-md5 264213ea4c5cb6b6d81ea97f59e757ab)
set(mkl-include-linux-64-md5 70b4f9a53401a3d11ce27d7ddb0e2511)
set(mkl-include-osx-64-md5 6da50c06992b78c4127a1881d39c1804)
set(mkl-include-win-64-md5 28d785eb22d28512d4e40e5890a817dc)
set(mkl-static-linux-64-md5 1469ad60a34269d4d0c5666bc131b82a)
set(mkl-static-osx-64-md5 4a099581ba95cc50bb538598b26389e4)
set(mkl-static-win-64-md5 69aef10428893314bc486e81397e1b25)
set(mkl-devel-linux-64-md5 2432ad963e3f7e4619ffc7f896178fbe)
set(mkl-devel-osx-64-md5 61b84a60715a3855a2097a3b619a00c8)
set(mkl-devel-win-64-md5 6128dee67d2b20ff534cf54757f623e0)

# To compute file names, we use the following bash script:
# for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-file") <(conda search --override-channel --channel intel $f=2021.3.0 --platform $os-64 -i | grep file | cut -d : -f 2); done; done
set(mkl-linux-64-file mkl-2021.3.0-intel_520.tar.bz2)
set(mkl-osx-64-file mkl-2021.3.0-intel_517.tar.bz2)
set(mkl-win-64-file mkl-2021.3.0-intel_524.tar.bz2)
set(mkl-include-linux-64-file mkl-include-2021.3.0-intel_520.tar.bz2)
set(mkl-include-osx-64-file mkl-include-2021.3.0-intel_517.tar.bz2)
set(mkl-include-win-64-file mkl-include-2021.3.0-intel_524.tar.bz2)
set(mkl-static-linux-64-file mkl-static-2021.3.0-intel_520.tar.bz2)
set(mkl-static-osx-64-file mkl-static-2021.3.0-intel_517.tar.bz2)
set(mkl-static-win-64-file mkl-static-2021.3.0-intel_524.tar.bz2)
set(mkl-devel-linux-64-file mkl-devel-2021.3.0-intel_520.tar.bz2)
set(mkl-devel-osx-64-file mkl-devel-2021.3.0-intel_517.tar.bz2)
set(mkl-devel-win-64-file mkl-devel-2021.3.0-intel_524.tar.bz2)
