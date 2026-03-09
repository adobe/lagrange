#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import lagrange


class TestVariant:
    def test_variant_exists(self):
        """Test that the variant attribute exists."""
        assert hasattr(lagrange, "variant"), "lagrange.variant attribute does not exist"

    def test_variant_is_string(self):
        """Test that the variant attribute is a string."""
        assert isinstance(lagrange.variant, str), (
            f"Expected variant to be str, got {type(lagrange.variant)}"
        )

    def test_variant_value(self):
        """Test that the variant attribute has a valid value."""
        valid_variants = ["corp", "open"]
        assert lagrange.variant in valid_variants, (
            f"Expected variant to be one of {valid_variants}, got {lagrange.variant!r}"
        )
