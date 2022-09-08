/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Logger.h>
#include <lagrange/ui/types/MDL.h>


#ifdef LAGRANGE_UI_USE_MDL
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/types/Texture.h>
#include <lagrange/utils/assert.h>
#include <tuple>

#include <mi/mdl_sdk.h>

#ifndef WIN32
#include <dlfcn.h>
#endif

namespace lagrange {
namespace ui {

using namespace mi::base;
using namespace mi::neuraylib;

template <class T>
static Handle<T> handle_of(T* x)
{
    return Handle<T>(x);
}

struct MDLImpl
{
    struct MDLState
    {
        Handle<INeuray> sdk;
        Handle<IMdl_compiler> compiler;
        Handle<IMdl_factory> factory;
        Handle<ITransaction> transaction;
    };

    MDLImpl()
    {
        // Load sdk
        state.sdk = handle_of(load_ineuray());
        la_runtime_assert(state.sdk, "Failed to load MDL library");

        // Set up compiler
        state.compiler = handle_of(state.sdk->get_api_component<IMdl_compiler>());

        // Load the FreeImage plugin.
        la_runtime_assert(
            state.compiler->load_plugin_library("nv_freeimage" MI_BASE_DLL_FILE_EXT) == 0,
            "Loading FreeImage");

        state.sdk->start();

        state.factory = handle_of(state.sdk->get_api_component<IMdl_factory>());

        /*
         * Load adobe's modules
         */

        // Create DB transaction
        auto database = handle_of(state.sdk->get_api_component<IDatabase>());
        auto scope = handle_of(database->get_global_scope());
        state.transaction = scope->create_transaction();

        auto context = handle_of(state.factory->create_execution_context());
        {
            const std::string mtl =
#include "../adobe_mdl/mtl.mdl"
                ;

            const std::string util =
#include "../adobe_mdl/util.mdl"
                ;

            const std::string annotations =
#include "../adobe_mdl/annotations.mdl"
                ;

            const std::string convert =
#include "../adobe_mdl/convert.mdl"
                ;

            mi::Sint32 err_code;
            err_code = state.compiler->load_module_from_string(
                state.transaction.get(),
                "::adobe::annotations",
                annotations.c_str(),
                context.get());
            if (err_code < 0) {
                logger().error("Failed to load module ::adobe::annotations, code: {}", err_code);
            }

            err_code = state.compiler->load_module_from_string(
                state.transaction.get(),
                "::adobe::convert",
                convert.c_str(),
                context.get());
            if (err_code < 0) {
                logger().error("Failed to load module ::adobe::convert, code: {}", err_code);
            }

            err_code = state.compiler->load_module_from_string(
                state.transaction.get(),
                "::adobe::mtl",
                mtl.c_str(),
                context.get());
            if (err_code < 0) {
                logger().error("Failed to load module ::adobe::mtl, code: {}", err_code);
            }

            err_code = state.compiler->load_module_from_string(
                state.transaction.get(),
                "::adobe::util",
                util.c_str(),
                context.get());
            if (err_code < 0) {
                logger().error("Failed to load module ::adobe::util, code: {}", err_code);
            }
        }

        state.transaction->commit();
    }

    Handle<const ITexture> get_texture(const IExpression* exp, const IType* type)
    {
        auto expression_constant = handle_of(exp->get_interface<IExpression_constant>());
        auto aliased_type = handle_of(type->skip_all_type_aliases());

        auto null = Handle<ITexture>(nullptr);

        if (!(expression_constant && aliased_type)) return null;

        auto kind = aliased_type->get_kind();

        if (kind != IType::TK_TEXTURE) return null;
        auto texture_value = handle_of(expression_constant->get_value<IValue_texture>());

        if (!texture_value) return null;

        auto texture = handle_of(state.transaction->access<ITexture>(texture_value->get_value()));
        if (!texture) return null;

        auto image = handle_of(state.transaction->access<IImage>(texture->get_image()));
        if (!image) return null;

        auto filename = image->get_filename();
        if (!filename) return null;

        return texture;
    }

    Resource<Texture> recursive_texture_call(const IExpression* exp)
    {
        auto expression_factory =
            handle_of(state.factory->create_expression_factory(state.transaction.get()));

        if (exp->get_kind() == IExpression::EK_CALL) {
            std::string call_name = reinterpret_cast<const IExpression_call*>(exp)->get_call();

            auto function_call =
                handle_of(state.transaction->access<IFunction_call>(call_name.c_str()));

            if (function_call) {
                auto types = handle_of(function_call->get_parameter_types());
                auto arguments = handle_of(function_call->get_arguments());

                logger().trace(
                    "\tCall name is {} param count: {}",
                    call_name,
                    function_call->get_parameter_count());

                for (size_t i = 0; i < function_call->get_parameter_count(); ++i) {
                    auto type = handle_of(types->get_type(i));
                    auto name = arguments->get_name(i);
                    if (!type) continue;
                    auto param_expression = handle_of(arguments->get_expression(i));

                    const std::string param_text =
                        handle_of(expression_factory->dump(param_expression.get(), 0, 1))
                            ->get_c_str();
                    logger().trace("{}", param_text);

                    switch (param_expression->get_kind()) {
                    case IExpression::EK_CALL: {
                        logger().trace("{}is a call", name);
                        return recursive_texture_call(param_expression.get());
                    }
                    case IExpression::EK_CONSTANT: {
                        logger().trace("{}is a constant", name);

                        auto tex = get_texture(param_expression.get(), type.get());
                        if (tex) {
                            auto image =
                                handle_of(state.transaction->access<IImage>(tex->get_image()));
                            if (!image) break;

                            auto filename = image->get_filename();
                            if (!filename) break;

                            Texture::Params p;
                            p.sRGB = (tex->get_effective_gamma() > 1.0f);

                            return Resource<Texture>::create(filename, p);

                            logger().trace("Got texture info !");
                        }
                        break;
                    }
                    default: break;
                    }
                }
            }
        }

        return {};
    }

    std::shared_ptr<Material> load_adobe_standard_single(const IModule* module, int index)
    {
        auto mat_ptr = std::make_shared<Material>();
        auto& mat = *mat_ptr;

        // Get material definition
        auto material_definition =
            handle_of(state.transaction->access<IMaterial_definition>(module->get_material(index)));

        if (!material_definition) return nullptr;

        // Factories for debugging
        auto expression_factory =
            handle_of(state.factory->create_expression_factory(state.transaction.get()));

        auto value_factory =
            handle_of(state.factory->create_value_factory(state.transaction.get()));

        mi::Size count = material_definition->get_parameter_count();
        auto defaults = handle_of(material_definition->get_defaults());
        auto param_types = handle_of(material_definition->get_parameter_types());

        // Iterate through default values of material's parameters
        for (mi::Size param_index = 0; param_index < count; param_index++) {
            std::string name = material_definition->get_parameter_name(param_index);

            auto default_ = handle_of(defaults->get_expression(name.c_str()));

            if (!default_.is_valid_interface()) continue;


            const auto kind = default_->get_kind();
            // If default is constant, copy it as value to material
            if (kind == IExpression::EK_CONSTANT) {
                auto value = handle_of(
                    reinterpret_cast<const IExpression_constant*>(default_.get())->get_value());

                auto value_kind = value->get_kind();
                switch (value_kind) {
                case IValue::Kind::VK_FLOAT:
                    mat[name].value = {
                        reinterpret_cast<const IValue_float*>(value.get())->get_value()};
                    break;
                case IValue::Kind::VK_DOUBLE:
                    mat[name].value = {
                        float(reinterpret_cast<const IValue_double*>(value.get())->get_value())};
                    break;
                case IValue::Kind::VK_COLOR: {
                    Color c = {0, 0, 0, 1};
                    auto color = reinterpret_cast<const IValue_color*>(value.get());

                    for (auto i = 0; i < 4; i++) {
                        auto element_value = handle_of(color->get_value(i));
                        if (!element_value) break;
                        c(i) = element_value->get_value();
                    }
                    mat[name].value = c;
                } break;
                default: logger().warn("Value {} not supported", name);
                };

                if (mat.has_map(name)) {
                    auto& map = mat[name];
                    logger().trace("{} Color({})", name, map.value.transpose());
                }
            }
            // Try to get texture and texture options
            else if (kind == IExpression::EK_CALL) {
                mat[name].texture = recursive_texture_call(default_.get());
            }
        }

        if (!mat.convert_to(Material::MATERIAL_ADOBE_STANDARD)) {
            logger().error("Material is not adobe's standard");
            return nullptr;
        }

        return mat_ptr;
    }

    MDL::Library load_adobe_standard(const fs::path& base_dir, const std::string& module_name)
    {
        MDL::Library result;

        if (state.compiler->add_module_path(base_dir.string().c_str()) != 0)
            logger().error("Failed to add module path");

        // Create DB transaction
        auto database = handle_of(state.sdk->get_api_component<IDatabase>());
        auto scope = handle_of(database->get_global_scope());
        state.transaction = scope->create_transaction();

        // Load Module
        auto context = handle_of(state.factory->create_execution_context());

        {
            if (state.compiler->load_module(
                    state.transaction.get(),
                    module_name.c_str(),
                    context.get()) < 0) {
                logger().error("Failed to load module");
                return {};
            }

            auto mat_module = handle_of(
                state.transaction->access<const IModule>(("mdl::" + module_name).c_str()));

            if (!mat_module.is_valid_interface()) {
                logger().error("Module is not valid");
                return {};
            }

            if (mat_module->get_material_count() == 0) {
                logger().error("Module has no materials");
                return {};
            }

            // Load individual materials
            for (auto matIndex = 0; matIndex < mat_module->get_material_count(); matIndex++) {
                auto mat_name_char = mat_module->get_material(matIndex);
                if (!mat_name_char) continue;

                auto mat = load_adobe_standard_single(mat_module.get(), matIndex);
                mat->set_name(mat_name_char);

                if (!mat) continue;

                std::string mat_name = mat_name_char;
                mat_name = mat_name.substr(mat_name.find_last_of(":") + 1);

                result.materials[mat_name] = std::move(mat);
            }
        }

        state.transaction->commit();

        return result;
    }

    ~MDLImpl()
    {
        state.factory = nullptr;
        state.transaction = nullptr;
        state.compiler = nullptr;
        state.sdk = nullptr;
        unload_lib(dll_handle);
    }

private:
    MDLState state;

    //From https://raytracing-docs.nvidia.com/mdl/api/html/mi_neuray_example_start_shutdown.html
    INeuray* load_ineuray(const char* filename = nullptr)
    {
        if (!filename) filename = "libmdl_sdk" MI_BASE_DLL_FILE_EXT;
#ifdef MI_PLATFORM_WINDOWS
        void* handle = LoadLibraryA((LPSTR)filename);
        if (!handle) {
            LPTSTR buffer = 0;
            LPCTSTR message = TEXT("unknown failure");
            DWORD error_code = GetLastError();
            if (FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                    0,
                    error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&buffer,
                    0,
                    0))
                message = buffer;
            logger().error("Failed to load library {}({}): {}", filename, error_code, message);
            if (buffer) LocalFree(buffer);
            return 0;
        }
        void* symbol = GetProcAddress((HMODULE)handle, "mi_factory");
        if (!symbol) {
            LPTSTR buffer = 0;
            LPCTSTR message = TEXT("unknown failure");
            DWORD error_code = GetLastError();
            if (FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                    0,
                    error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&buffer,
                    0,
                    0))
                message = buffer;
            logger().error("GetProcAddress error mi_factory({}): {}", error_code, message);
            if (buffer) LocalFree(buffer);
            return 0;
        }
#else // MI_PLATFORM_WINDOWS
        void* handle = dlopen(filename, RTLD_LAZY);
        if (!handle) {
            fprintf(stderr, "%s\n", dlerror());
            return 0;
        }
        void* symbol = dlsym(handle, "mi_factory");
        if (!symbol) {
            fprintf(stderr, "%s\n", dlerror());
            return 0;
        }
#endif // MI_PLATFORM_WINDOWS
        dll_handle = handle;
        INeuray* neuray = mi_factory<INeuray>(symbol);
        if (!neuray) {
            Handle<IVersion> version(mi_factory<IVersion>(symbol));
            if (!version)
                fprintf(stderr, "Error: Incompatible library.\n");
            else
                fprintf(
                    stderr,
                    "Error: Library version %s does not match header version %s.\n",
                    version->get_product_version(),
                    MI_NEURAYLIB_PRODUCT_VERSION_STRING);
            return 0;
        }
        return neuray;
    }

    bool unload_lib(void* handle)
    {
#ifdef MI_PLATFORM_WINDOWS
        int result = FreeLibrary((HMODULE)dll_handle);
        if (result == 0) {
            LPTSTR buffer = 0;
            LPCTSTR message = TEXT("unknown failure");
            DWORD error_code = GetLastError();
            if (FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                    0,
                    error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&buffer,
                    0,
                    0))
                message = buffer;
            logger().error("Failed to unload library ({}): {}", error_code, message);
            if (buffer) LocalFree(buffer);
            return false;
        }
        return true;
#else
        int result = dlclose(dll_handle);
        if (result != 0) {
            printf("%s\n", dlerror());
            return false;
        }
        return true;
#endif
        return true;
    }

    void* dll_handle;
};

MDL::MDL()
    : m_impl(std::make_unique<MDLImpl>())
{}

MDL::~MDL() {}

MDL& MDL::get_instance()
{
    if (!m_instance) m_instance = std::make_unique<MDL>();

    return *m_instance;
}

MDL::Library MDL::load_materials(const fs::path& base_dir, const std::string& module_name)
{
    return m_impl->load_adobe_standard(base_dir, module_name);
}

std::unique_ptr<MDL> MDL::m_instance = nullptr;

} // namespace ui
} // namespace lagrange

#else
namespace lagrange {
namespace ui {

struct MDLImpl
{
};

MDL::MDL()
    : m_impl(nullptr)
{
    logger().error("MDL Support not enabled, compile with LAGRANGE_UI_USE_MDL");
}

MDL::~MDL() {}

MDL& MDL::get_instance()
{
    if (!m_instance) m_instance = std::make_unique<MDL>();

    return *m_instance;
}

MDL::Library MDL::load_materials(const fs::path& base_dir, const std::string& module_name)
{
    (void)(base_dir);
    (void)(module_name);
    return {};
}

std::unique_ptr<MDL> MDL::m_instance = nullptr;

} // namespace ui
} // namespace lagrange

#endif
