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
import re
from pathlib import Path
import argparse


def fix_file(filename):
    with open(filename, "r+") as fin:
        content = fin.read()

        # Replace "numpy.ndarray[...]" to "numpy.typing.NDArray"
        content = re.sub(
            r"numpy.ndarray\[.*\]", "numpy.typing.NDArray", content
        )

        # Replace "<$type $object at $address>" to "..."
        content = re.sub(r"<.* at 0x[0-9a-f]+>", "...", content)

        fin.seek(0)
        fin.write(content)
        fin.truncate()


def add_submodules(init_file, submodules):
    with open(init_file, "a") as fin:
        for module in submodules:
            fin.write(f"import lagrange.{module}\n")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Fixes stub files for nanobind-stubgen"
    )
    parser.add_argument("path", help="Path to the file or directory to fix")
    return parser.parse_args()


def main():
    args = parse_args()
    path = Path(args.path)
    if path.is_dir():
        submodules = []
        for file in path.glob("*.pyi"):
            fix_file(file)
            if file.name != "__init__.pyi":
                submodules.append(file.stem)

        init_file = path / "__init__.pyi"
        if init_file.exists():
            add_submodules(init_file, submodules)

    else:
        fix_file(path)


if __name__ == "__main__":
    main()
