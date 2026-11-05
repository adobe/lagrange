/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>

#include <lagrange/internal/SafeVector.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

TEST_CASE("SafeVector: nested value copies", "[utils][SafeVector]")
{
    using Vector = lagrange::SafeVector<lagrange::SafeVector<std::string>>;
    Vector source;
    source.emplace_back().push_back("source");
    source.emplace_back();

    Vector copy;
    SECTION("copy construction")
    {
        copy = Vector(source);
    }
    SECTION("copy assignment replaces existing elements")
    {
        copy.emplace_back().push_back("old");
        copy.emplace_back().push_back("discarded");
        copy.emplace_back();
        copy = source;
    }

    REQUIRE(copy.size() == 2);
    REQUIRE(copy[0].size() == 1);
    CHECK(copy[0][0] == "source");
    CHECK(copy[1].empty());

    copy[0][0] = "copy";
    copy[1].push_back("new");
    CHECK(source[0][0] == "source");
    CHECK(source[1].empty());
    source[0].push_back("original only");
    CHECK(copy[0].size() == 1);

    auto* element = &copy[0][0];
    const auto& alias = copy;
    copy = alias;
    CHECK(&copy[0][0] == element);
    CHECK(copy[0][0] == "copy");
}

TEST_CASE("SafeVector: moves preserve element identity", "[utils][SafeVector]")
{
    using Vector = lagrange::SafeVector<std::unique_ptr<int>>;
    static_assert(std::is_nothrow_move_constructible_v<Vector>);
    static_assert(std::is_nothrow_move_assignable_v<Vector>);

    Vector source;
    source.push_back(std::make_unique<int>(42));
    auto* element = &source[0];
    auto* pointee = source[0].get();

    Vector destination;
    SECTION("move construction")
    {
        destination = Vector(std::move(source));
    }
    SECTION("move assignment replaces existing elements")
    {
        destination.push_back(std::make_unique<int>(7));
        destination = std::move(source);
    }

    REQUIRE(destination.size() == 1);
    CHECK(&destination[0] == element);
    CHECK(destination[0].get() == pointee);
    CHECK(*destination[0] == 42);
}

#ifdef LAGRANGE_WITH_PYTHON
namespace {
struct ThrowOnCopy
{
    int value;
    bool fail;

    ThrowOnCopy(int value_, bool fail_ = false)
        : value(value_)
        , fail(fail_)
    {}

    ThrowOnCopy(const ThrowOnCopy& other)
        : value(other.value)
        , fail(other.fail)
    {
        if (fail) throw std::runtime_error("element copy failed");
    }
};
} // namespace

TEST_CASE("SafeVector: throwing copy assignment preserves destination", "[utils][SafeVector]")
{
    lagrange::SafeVector<ThrowOnCopy> source;
    source.emplace_back(1);
    source.emplace_back(2, true);
    lagrange::SafeVector<ThrowOnCopy> destination;
    auto* original = &destination.emplace_back(3);

    REQUIRE_THROWS_AS(destination = source, std::runtime_error);
    REQUIRE(destination.size() == 1);
    CHECK(&destination[0] == original);
    CHECK(destination[0].value == 3);
    CHECK(source[0].value == 1);
    CHECK(source[1].value == 2);
}

TEST_CASE("SafeVector: copies preserve null storage slots", "[utils][SafeVector]")
{
    using Vector = lagrange::SafeVector<std::string>;
    Vector source;
    source.resize(1);
    source.push_back("value");

    Vector copy(source);
    Vector assigned;
    assigned = source;
    CHECK(static_cast<const Vector::Super&>(copy)[0] == nullptr);
    CHECK(static_cast<const Vector::Super&>(assigned)[0] == nullptr);
    copy[1] = "copied";
    assigned[1] = "assigned";
    CHECK(source[1] == "value");
}
#endif
