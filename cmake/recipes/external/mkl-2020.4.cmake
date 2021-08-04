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
# for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-md5") <(conda search --override-channel --channel intel $f=2020.4 --platform $os-64 -i | grep md5 | cut -d : -f 2); done; done
set(mkl-linux-64-md5 f85891f97a04c7b2fbf619d1b903d7f5)
set(mkl-osx-64-md5 735d7f93c7fbcffe658f1ccf67418cb3)
set(mkl-win-64-md5 37ade09cace5cd73053b16574a3ee3c3)
set(mkl-include-linux-64-md5 8b2bf0e42bd95dd700d9877add1ca6de)
set(mkl-include-osx-64-md5 26043328553952cdb064c5aab8b50c78)
set(mkl-include-win-64-md5 87e9a73a6e6757a8ed0dbc87d50d7f60)
set(mkl-static-linux-64-md5 9f589a1508fb083c3e73427db459ca4c)
set(mkl-static-osx-64-md5 2f9e1b8b6d6b0903e81a573084e4494f)
set(mkl-static-win-64-md5 5ae780c06edd0be62966c6d8ab47d5fb)
set(mkl-devel-linux-64-md5 b571698ef237c0e61abe15b7d300f157)
set(mkl-devel-osx-64-md5 ee58da0463676d910eeab9aec0470f0e)
set(mkl-devel-win-64-md5 8a7736b81b9bc2d5c044b88d6ac8af6e)

# To compute file names, we use the following bash script:
# for f in mkl mkl-include mkl-static mkl-devel; do for os in linux osx win; do cat <(printf "$f-$os-64-file") <(conda search --override-channel --channel intel $f=2020.4 --platform $os-64 -i | grep file | cut -d : -f 2); done; done
set(mkl-linux-64-file mkl-2020.4-intel_304.tar.bz2)
set(mkl-osx-64-file mkl-2020.4-intel_301.tar.bz2)
set(mkl-win-64-file mkl-2020.4-intel_311.tar.bz2)
set(mkl-include-linux-64-file mkl-include-2020.4-intel_304.tar.bz2)
set(mkl-include-osx-64-file mkl-include-2020.4-intel_301.tar.bz2)
set(mkl-include-win-64-file mkl-include-2020.4-intel_311.tar.bz2)
set(mkl-static-linux-64-file mkl-static-2020.4-intel_304.tar.bz2)
set(mkl-static-osx-64-file mkl-static-2020.4-intel_301.tar.bz2)
set(mkl-static-win-64-file mkl-static-2020.4-intel_311.tar.bz2)
set(mkl-devel-linux-64-file mkl-devel-2020.4-intel_304.tar.bz2)
set(mkl-devel-osx-64-file mkl-devel-2020.4-intel_301.tar.bz2)
set(mkl-devel-win-64-file mkl-devel-2020.4-intel_311.tar.bz2)
