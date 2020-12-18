/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/ui/Resource.h>

using namespace lagrange::ui;

TEST_CASE("ResourceStringConversion", "[ui][resource]")
{
    ResourceFactory::clear();

    struct A
    {
        A(const std::string& str)
            : string_data(str)
        {}

        std::string string_data;
    };

    ResourceFactory::register_resource_factory(
        [](ResourceData<A>& data, const std::string& str) { data.set(std::make_unique<A>(str)); });

    // Direct

    SECTION("const char *")
    {
        auto res = Resource<A>::create("test_string");
        REQUIRE(res->string_data == "test_string");
    }

    SECTION("const std::string &")
    {
        auto res = Resource<A>::create(std::string("test_string"));
        REQUIRE(res->string_data == "test_string");
    }

    SECTION("std::string &&")
    {
        std::string str("test_string");
        auto res = Resource<A>::create(std::move(str));
        REQUIRE(res->string_data == "test_string");
    }

    // Deferred

    SECTION("const char * (deferred)")
    {
        auto res = Resource<A>::create_deferred("test_string");
        REQUIRE(res->string_data == "test_string");
    }

    SECTION("const std::string & (deferred)")
    {
        auto res = Resource<A>::create_deferred(std::string("test_string"));
        REQUIRE(res->string_data == "test_string");
    }

    SECTION("std::string && (deferred)")
    {
        std::string str("test_string");
        auto res = Resource<A>::create_deferred(std::move(str));
        REQUIRE(res->string_data == "test_string");
    }
}

namespace {

void test_realization(bool deferred)
{
    std::string postfix = deferred ? " (deferred)" : " (direct)";

    SECTION("POD default constructor" + postfix)
    {
        auto res = deferred ? Resource<int>::create_deferred(42) : Resource<int>::create(42);
        REQUIRE(*res == 42);
    }


    SECTION("struct default constructor" + postfix)
    {
        struct A
        {
            A(int in)
                : x(in)
            {}
            int x = 0;
        };

        auto res = deferred ? Resource<A>::create_deferred(42) : Resource<A>::create(42);
        REQUIRE(res->x == 42);
    }


    SECTION("struct factory constructors" + postfix)
    {
        struct A
        {
            int x = 0;
        };

        ResourceFactory::register_resource_factory([](ResourceData<A>& data, int x) {
            auto a = std::make_unique<A>();
            a->x = x;
            data.set(std::move(a));
        });

        // Via factory
        {
            auto res = deferred ? Resource<A>::create_deferred(42) : Resource<A>::create(42);
            REQUIRE(res->x == 42);
        }

        // Copy constructor
        {
            A existing;
            existing.x = 42;

            auto res =
                deferred ? Resource<A>::create_deferred(existing) : Resource<A>::create(existing);
            REQUIRE(res->x == 42);
        }

        // Move constructor
        {
            A existing;
            existing.x = 42;

            auto res = deferred ? Resource<A>::create_deferred(std::move(existing))
                                : Resource<A>::create(std::move(existing));
            REQUIRE(res->x == 42);
        }

        // Shared ptr data
        {
            auto existing = std::make_shared<A>();
            existing->x = 42;

            auto res =
                deferred ? Resource<A>::create_deferred(existing) : Resource<A>::create(existing);
            REQUIRE(res->x == 42);
        }

        // Unique ptr data
        {
            auto existing = std::make_unique<A>();
            existing->x = 42;

            auto res = deferred ? Resource<A>::create_deferred(std::move(existing))
                                : Resource<A>::create(std::move(existing));
            REQUIRE(res->x == 42);
        }
    }
}

} // namespace


TEST_CASE("ResourceRealization", "[ui][resource]")
{
    ResourceFactory::clear();

    test_realization(true);
    test_realization(false);
}


TEST_CASE("ResourceNoncopyable", "[ui][resource]")
{
    ResourceFactory::clear();

    // Limitation: Non copyable parameters cannot be used with create_deferred()

    SECTION("Non copyable struct")
    {
        struct A
        {
            std::unique_ptr<int> x_ptr;
        };

        ResourceFactory::register_resource_factory([](ResourceData<A>& data, int x) {
            auto a = std::make_unique<A>();
            a->x_ptr = std::make_unique<int>(x);
            data.set(std::move(a));
        });

        auto res = Resource<A>::create(42);

        REQUIRE(*res->x_ptr == 42);
    }


    SECTION("Non copyable parameter pack")
    {
        struct A
        {
            A(int in)
                : x(in)
            {}
            int x;
        };

        ResourceFactory::register_resource_factory([](ResourceData<A>& data,
                                                      std::unique_ptr<int> first_arg,
                                                      std::unique_ptr<int> second_arg) {
            auto a = std::make_unique<A>((*first_arg) + (*second_arg));
            data.set(std::move(a));
        });

        auto first_arg = std::make_unique<int>(40);
        auto second_arg = std::make_unique<int>(2);

        auto res = Resource<A>::create(std::move(first_arg), std::move(second_arg));
        REQUIRE(res->x == 42);
    }


    SECTION("Non copyable parameter and struct")
    {
        struct A
        {
            std::unique_ptr<int> x_ptr;
        };

        ResourceFactory::register_resource_factory(
            [](ResourceData<A>& data, std::unique_ptr<int> ptr) {
                auto a = std::make_unique<A>();
                a->x_ptr = std::move(ptr);
                data.set(std::move(a));
            });

        auto res = Resource<A>::create(std::make_unique<int>(42));

        REQUIRE(*res->x_ptr == 42);
    }
}


TEST_CASE("ResourceDeferredReload", "[ui][resource]")
{
    ResourceFactory::clear();

    SECTION("Copyable parameter")
    {
        auto res = Resource<int>::create_deferred(42);
        REQUIRE(*res == 42);

        // Set value to something else
        *res = 0;

        // Reload
        REQUIRE_NOTHROW(res.reload());

        REQUIRE(*res == 42);
    }

    SECTION("Moved parameter")
    {
        struct B
        {
            B() = delete;
            B(const std::string& value)
                : str(value)
            {}
            std::string str;
        };
        struct A
        {
            A(B&& b_rvalue)
                : b(b_rvalue)
            {}
            B b;
        };


        ResourceFactory::register_resource_factory([](ResourceData<A>& data, B&& b) {
            auto a = std::make_unique<A>(std::move(b));
            data.set(std::move(a));
        });


        // Implicit move
        {
            auto res = Resource<A>::create_deferred(B("test_string"));
            REQUIRE(res->b.str == "test_string");

            auto saved_param = std::any_cast<std::tuple<B>>(res.data()->params());
            REQUIRE(std::get<0>(saved_param).str != "test_string");

            res.reload();

            REQUIRE(res->b.str != "test_string");
        }

        // Copy
        {
            auto b = B("test_string");
            auto res = Resource<A>::create_deferred(b);
            REQUIRE(res->b.str == "test_string");
            REQUIRE(b.str == "test_string");

            res.reload();

            REQUIRE(res->b.str != "test_string");
        }

        // Move
        {
            auto b = B("test_string");
            auto res = Resource<A>::create_deferred(std::move(b));
            REQUIRE(res->b.str == "test_string");
            REQUIRE(b.str != "test_string");

            res->b.str = "";
            res.reload();

            REQUIRE(res->b.str != "test_string");
        }
    }
}


TEST_CASE("ResourceDependencies", "[ui][resource]")
{
    ResourceFactory::clear();

    SECTION("Single level")
    {
        struct A
        {
            int value = 0;
        };

        struct B
        {
            Resource<A> a;
            int value = 0;
        };

        ResourceFactory::register_resource_factory([](ResourceData<B>& data, Resource<A> ares) {
            data.set(std::make_unique<B>(B{ares, ares->value * 2}));
            data.add_dependency(ares.data());
        });

        A initial_a = {42};

        auto Ares = Resource<A>::create_deferred(initial_a);
        auto Bres = Resource<B>::create_deferred(Ares);

        // std::vector<std::shared_ptr<BaseResourceData>> dirty_deps;

        REQUIRE(Bres->value == Ares->value * 2);

        Ares->value = 10;
        Ares.set_dirty(true);

        // Invalid state
        REQUIRE(Bres->value != Ares->value * 2);


        // Check one level of dependencies, reload if needed
        for (auto dep : Bres.dependencies()) {
            if (dep->is_dirty()) {
                Bres.reload();
            }
        }

        REQUIRE(Bres->value == Ares->value * 2);
    }


    SECTION("Linked list")
    {
        struct A
        {
            Resource<A> prev;
            int value = 0;
        };

        ResourceFactory::register_resource_factory([](ResourceData<A>& data, Resource<A> prev) {
            auto current = std::make_unique<A>();
            current->prev = prev;
            current->value = prev->value + 1;
            data.set(std::move(current));
            data.add_dependency(prev.data());
        });


        auto first = Resource<A>::create();
        auto prev = first;

        for (auto i = 0; i < 100; i++) {
            prev = Resource<A>::create_deferred(prev);
        }

        auto last = prev;
        REQUIRE(last->value == 100);


        first->value = 1;

        // No change, as its not marked dirty
        {
            last.check_and_reload_dependencies();
            REQUIRE(last->value == 100);
        }

        first.set_dirty(true);

        // Change should be propagated now
        {
            last.check_and_reload_dependencies();
            REQUIRE(last->value == 101);
        }
    }
}

namespace {
struct CounterType
{
    CounterType()
        : has_data(true)
    {
        lagrange::logger().trace("Default Constructor {}", has_data);
        default_construct_counter++;
    }
    ~CounterType() { lagrange::logger().trace("Destructor {}", has_data); }
    CounterType(const CounterType& other)
    {
        has_data = other.has_data;
        copy_counter++;
        lagrange::logger().trace("Copy Constructor {}", has_data);
    }
    CounterType(CounterType&& other)
    {
        if (other.has_data) {
            has_data = true;
            other.has_data = false;
        }
        lagrange::logger().trace("Move Constructor {}", has_data);
    }
    CounterType& operator=(const CounterType& other)
    {
        has_data = other.has_data;
        copy_counter++;
        lagrange::logger().trace("Copy Assignment {}", has_data);
        return *this;
    }
    CounterType& operator=(CounterType&& other)
    {
        if (other.has_data) {
            has_data = true;
            other.has_data = false;
        }
        lagrange::logger().trace("Move Assignment {}", has_data);
        return *this;
    }

    bool has_data = false;

    static int copy_counter;
    static int default_construct_counter;
};

int CounterType::copy_counter = 0;
int CounterType::default_construct_counter = 0;

} // namespace


TEST_CASE("ResourceCopyCounter", "[ui][resource]")
{
    ResourceFactory::clear();

    ResourceFactory::register_resource_factory([](ResourceData<int>& data, CounterType&& ct) {
        LA_IGNORE(data);
        LA_IGNORE(ct);
        lagrange::logger().trace("counter factory called!");
    });

    /*
        Make sure no unnecessary copies are made,
        e.g., when parameter to the factory function is a heavy type like a large Eigen array
    */
    SECTION("Deferred")
    {
        CounterType::copy_counter = 0;
        CounterType::default_construct_counter = 0;

        CounterType c;
        lagrange::logger().trace("starting deferred");

        auto res = Resource<int>::create_deferred(std::move(c));

        // Dereference to load
        (*res);

        REQUIRE(CounterType::copy_counter == 0);
        REQUIRE(CounterType::default_construct_counter == 1);
    }

    SECTION("Direct")
    {
        CounterType::copy_counter = 0;
        CounterType::default_construct_counter = 0;

        CounterType c;
        lagrange::logger().trace("starting direct");

        auto res = Resource<int>::create(std::move(c));

        REQUIRE(CounterType::copy_counter == 0);
        REQUIRE(CounterType::default_construct_counter == 1);
    }
}


TEST_CASE("ResourceReloadWith", "[ui][resource]")
{
    SECTION("int")
    {
        auto res = Resource<int>::create(42);

        REQUIRE(*res == 42);

        res.reload_with(43);

        REQUIRE(*res == 43);
    }

    SECTION("std::string")
    {
        auto res = Resource<std::string>::create("test_string");

        REQUIRE(*res == "test_string");

        res.reload_with("another_string");

        REQUIRE(*res == "another_string");
    }


    SECTION("rvalue, no default constructor")
    {
        struct A
        {
            A() = delete;
            A(int in)
                : x(in)
            {}
            int x;
        };

        struct B
        {
            B(A&& a_in)
                : a(std::move(a_in))
            {}
            A a;
        };


        ResourceFactory::register_resource_factory(
            [](ResourceData<B>& data, A&& a) { data.set(std::make_unique<B>(std::move(a))); });

        auto res = Resource<B>::create(A(42));

        REQUIRE(res->a.x == 42);

        res.reload_with(A(43));

        REQUIRE(res->a.x == 43);
    }
}
