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
#pragma once

#include <lagrange/ui/ResourceData.h>
#include <lagrange/ui/ResourceFactory.h>
#include <lagrange/ui/ResourceUtils.h>


namespace lagrange {
namespace ui {


// Forward declaration
template <typename T>
class Resource;

namespace detail {

// Creates a resource handle, resource initialization is deferred
template <typename T, typename... Args>
Resource<T> create_resource_deferred(Args... args);


/// Save parameters if they are copyable, realize using deferred mechanism
/// This will move out any rvalue references
template <typename T, typename... Args>
Resource<T> create_resource_direct(
    Args... args,
    typename std::enable_if<
        std::is_constructible<std::tuple<Args...>, const std::tuple<Args...>&>::value>::type*
        dummy = 0);

/// Do not save parameters if they are not copyable
template <typename T, typename... Args>
Resource<T> create_resource_direct(
    Args... args,
    typename std::enable_if<
        !std::is_constructible<std::tuple<Args...>, const std::tuple<Args...>&>::value>::type*
        dummy = 0);

} // namespace detail


/// Resource base class
class BaseResource
{
public:
    BaseResource() = default;
    virtual ~BaseResource() = default;

    virtual void reload() = 0;

    virtual bool has_value() const = 0;
    virtual size_t use_count() const = 0;

private:
};


/// Resource handle
/// Can either be created directly using create(args ...)
/// or the initialization can be deferred using create_deferred(args...)
///
/// Usage example:
/// ```
/// Resource<int> x = Resource<int>::create(0); //Creates int resource with value = 0
/// *x == 1 //Sets value to 1
/// ```
/// 
/// Deferred intialization
/// ```
/// Resource<int> x = Resource<int>::create_deferred(0); //Creates int resource, is not initialized
/// print(*x) //Resource is initialized on first use (first dereference), prints 0
/// ```
///
/// Example of custom factory function:
/// ```
/// ResourceFactory::register_resource_factory([](ResourceData<int> & data, CustomType x){
///     data.set(std::make_unique<int>(x.func_returning_int()));
/// });
/// ```
///
/// Example of custom factory function that consumes rvalue (no copies are performed):
/// ```
/// ResourceFactory::register_resource_factory([](ResourceData<MyType> & data, SomeBigType && x){
///     data.set(std::make_unique<MyType>(std::move(SomeBigType)));
/// });
/// SomeBigType big_object;
/// auto my_type_resource = Resource<MyType>::create_deferred(std::move(big_object)); //big object is saved as a parameter (via move)
/// my_type_resource->do_something(); //when resource is dereferenced for the first time, the above factory is called and the big object is moved to MyType
/// ```
///
template <typename T>
class Resource : public BaseResource
{
public:
    /// Creates empty resource
    Resource()
        : m_value(std::make_unique<ResourceData<T>>())
    {}

    /// Creates resource with existing resource data
    Resource(std::shared_ptr<ResourceData<T>> value)
        : m_value(value)
    {
        LA_ASSERT(value, "Resource pointer must be set");
    }

    /// Creates resource handle, args will be saved and the resource will be created on first use
    template <typename... Args>
    static Resource<T> create_deferred(Args... args)
    {
        return detail::create_resource_deferred<T, Args...>(std::forward<Args>(args)...);
    }

    /// Create resource handle, resource will be immediately created with given args
    template <typename... Args>
    static Resource<T> create(Args... args)
    {
        return detail::create_resource_direct<T, Args...>(std::forward<Args>(args)...);
    }

    /// Returns a pointer to resource
    T* get() const
    {
        T* ptr = m_value->get();
        if (ptr) return ptr;

        realize_value();
        return m_value->get();
    }

    T& operator*() const { return *get(); }
    T* operator->() const { return get(); }
    operator bool() const { return has_value(); }

    // Either has been realized, or has parameters and we try to realize it now
    bool has_value() const override
    {   
        return (m_value->get() != nullptr) || (m_value->params().has_value() && get() != nullptr);
    };

    template <typename K>
    K & cast() const
    {
        return *dynamic_cast<K*>(get());
    }

    template <typename K>
    K* try_cast() const
    {
        return dynamic_cast<K*>(get());
    }


    /// Parameter tuple used to create this resource
    /// Will return empty std::any if the resource was initialized directly
    const std::any& get_params() const { return m_value->params(); }

    /// Number of references to this resource's data
    size_t use_count() const override { return m_value.use_count(); }

    /// Returns internal data pointer
    std::shared_ptr<ResourceData<T>> data() const { return m_value; }

    /// Marks the resource as dirty
    /// Use this everytime you change the resource and need to propagate changes
    /// to resources that depend on this one
    /// To automatically reload dirty resources, use resource.check_and_reload_dependencies()
    void set_dirty(bool value) { m_value->set_dirty(value); }

    /// Returns whether the dirty flag was set
    bool is_dirty() const { return m_value->is_dirty(); }

    /// Returns pointers to data of dependent resources
    const std::vector<std::shared_ptr<BaseResourceData>>& dependencies() const
    {
        return m_value->dependencies();
    }

    /// Reloads the resource and marks it dirty
    /// Throws if the resource was not created as deferred (and therefore does not have parameters saved)
    void reload() override { m_value->reload(); }

    /// Explicitly loads the resource
    void load()
    {
        LA_ASSERT(!has_value(), "Resource is already loaded, call reload instead");
        realize_value();
    }

    /// Reloads the resource with given parameters and marks it dirty
    template <typename... Args>
    void reload_with(Args... args)
    {
        // Free old value
        m_value->set(nullptr);

        // Clear old parameters and dependencies
        m_value->clear_params();
        m_value->clear_dependencies();

        // Realize using given arguments
        ResourceFactory::realize<T>(*m_value, std::forward<Args>(args)...);

        set_dirty(true);
    }

    /// Resets this handle, there are no parameters or value stored after this call
    void reset() { m_value = std::make_shared<ResourceData<T>>(); }

    /// Traverses dependency tree and reloads it if any resource was marked dirty
    void check_and_reload_dependencies()
    {
        std::function<bool(BaseResourceData * current)> recursive_fn;

        recursive_fn = [&](BaseResourceData* current) {
            bool do_reload = current->is_dirty();

            // Leaf
            if (do_reload && current->dependencies().size() == 0) {
                current->set_dirty(false);
                return true;
            } else {
                for (auto dep : current->dependencies()) {
                    do_reload |= recursive_fn(dep.get());
                }
            }

            if (do_reload) {
                current->reload();
                current->set_dirty(false);
                return true;
            }

            return false;
        };

        recursive_fn(data().get());
    }


private:
    void realize_value() const { m_value->realize(); }

    std::shared_ptr<ResourceData<T>> m_value;
};


namespace detail {
template <typename T, typename... Args>
Resource<T> create_resource_deferred(Args... args)
{
    // Register factory that simply forwards Args to T's constructor
    ResourceFactory::register_constructor_forwarding_factory<T, Args...>();

    // Save arguments
    return std::make_shared<ResourceData<T>>(
        std::forward<detail::convert_implicit_t<Args>>(args)...);
}


template <typename T, typename... Args>
Resource<T> create_resource_direct(
    Args... args,
    typename std::enable_if<
        std::is_constructible<std::tuple<Args...>, const std::tuple<Args...>&>::value>::type* dummy)
{
    (void)(dummy);
    // Create as deferred
    auto resource = create_resource_deferred<T>(std::forward<Args>(args)...);

    // Immediately realize
    (*resource);

    return resource;
}


template <typename T, typename... Args>
Resource<T> create_resource_direct(
    Args... args,
    typename std::enable_if<
        !std::is_constructible<std::tuple<Args...>, const std::tuple<Args...>&>::value>::type*
        dummy)
{
    (void)(dummy);

    auto resource_ptr = std::make_shared<ResourceData<T>>();

    ResourceFactory::realize<T>(*resource_ptr, std::forward<Args>(args)...);

    return Resource<T>(resource_ptr);
}

} // namespace detail


} // namespace ui
} // namespace lagrange
