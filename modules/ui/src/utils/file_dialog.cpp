/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/ui/utils/file_dialog.h>

#include <lagrange/Logger.h>
#include <portable-file-dialogs.h>

#if defined(__EMSCRIPTEN__)
    #include <emscripten.h>
#endif


namespace lagrange {
namespace ui {

#if defined(__EMSCRIPTEN__)
///
/// Triggers download in the browser
///
/// @param[in]  title          Filename in browser's file system
/// @param[in]  mime_type      MIME Type of the file, see:
//                             https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
///
/// @return  True if download triggered successfully
///
bool download_file(const fs::path& path, const std::string& mime_type = "application/octet-stream")
{
    const std::string path_str = path.string();
    int result = EM_ASM_INT(
        {
            let filename = UTF8ToString($0);
            let mime = UTF8ToString($1);

            var content = "";
            try {
                content = FS.readFile(filename);
            } catch (ex) {
                console.error(
                    "Exception occured while reading file " + filename + ". Exception: " + ex);
                return 1;
            }

            console.debug("Offering download of " + filename);

            var a = document.createElement('a');
            a.download = filename;
            a.href = URL.createObjectURL(new Blob([content], {
                type:
                    mime
            }));
            a.style.display = 'none';

            document.body.appendChild(a);
            a.click();
            setTimeout(
                function() {
                    document.body.removeChild(a);
                    URL.revokeObjectURL(a.href);
                },
                2000);

            return 0;
        },
        path_str.c_str(),
        mime_type.c_str());

    return (result == 0);
}

///
/// Closes files in the browser (deletes them from the browser's temporary filesystem)
///
/// @param[in]  files          File paths to close
///
/// @return  True if all files were closed successfully.
///
bool close_files(const std::vector<fs::path>& files)
{
    if (files.size() == 0) return true;

    // Concatenate file list
    std::string file_list;
    for (size_t i = 0; i < files.size(); i++) {
        file_list += files[i].string();
        if (i < files.size() - 1) {
            file_list += ';';
        }
    }

    int result = EM_ASM_INT(
        {
            let file_list = UTF8ToString($0);
            if (file_list == null) {
                return 0;
            }

            let result = 0;
            const files = file_list.split(";");
            for (var i = 0; i < files.length; i++) {
                try {
                    FS.unlink(files[i])
                } catch (exception) {
                    console.error("Failed to delete " + files[i] + ": " + exception);
                    result = 1;
                }
            }
            return result;
        },
        file_list.c_str());

    return (result == 0);
}
#endif


/// Shared handle to manage temporary filesystem file lifetime
struct FileDialogPath::FileDialogPathImpl
{
    FileDialogPathImpl(const fs::path& input_path)
        : path(input_path)
    {}

    ~FileDialogPathImpl()
    {
        if (path.empty()) return;

#if defined(__EMSCRIPTEN__)
        if (!is_input) {
            download_file(path);
        }
        close_files({path});
#endif
    }

    fs::path path = "";
    bool is_input = false;
};

FileDialogPath FileDialogPath::make_input_path(const fs::path& path)
{
    FileDialogPath fp(path);
    fp.m_impl->is_input = true;
    return fp;
}

FileDialogPath FileDialogPath::make_output_path(const fs::path& path)
{
    FileDialogPath fp(path);
    fp.m_impl->is_input = false;
    return fp;
}

FileDialogPath::FileDialogPath(const fs::path& path)
    : m_impl(std::make_unique<FileDialogPath::FileDialogPathImpl>(path))
{}


FileDialogPath::FileDialogPath(FileDialogPath&& other) = default;
FileDialogPath& FileDialogPath::operator=(FileDialogPath&& other) = default;
FileDialogPath::~FileDialogPath() = default;

const fs::path& FileDialogPath::path() const
{
    return m_impl->path;
}


namespace utils {

std::vector<std::string> transform_filters(const std::vector<FileFilter>& filters)
{
    std::vector<std::string> results;
    results.reserve(2 * filters.size());
    for (const auto& filter : filters) {
        results.push_back(filter.name);
        results.push_back(filter.pattern);
    }
    return results;
}


std::string transform_filters_to_accept(const std::vector<FileFilter>& filters)
{
    const auto regex_str = "(([*]*\\.[a-zA-Z0-9_]+)|(audio\\/\\*)|(video\\/\\*)|(image\\/"
                           "\\*))|((application|audio|font|example|image|message|model|multipart|"
                           "text|video)\\/([0-9A-Za-z!#$%&'*+.^_`|~-]+))";

    static const auto regex = std::regex(regex_str);

    std::string result;

    for (const auto& filter : filters) {
        if (filter.pattern == "*") return "";

        // filter.pattern
        auto search_start = filter.pattern.cbegin();
        std::smatch match;
        while (std::regex_search(search_start, filter.pattern.cend(), match, regex)) {
            // Append without wildcard symbol
            if (match[0].str().front() == '*') {
                result.append(match[0].str().data() + 1, match[0].length() - 1);
            } else {
                result.append(match[0]);
            }
            result.append(",");
            search_start = match.suffix().first;
        }
    }

    // Omit last ','
    if (result.length() > 0) result.resize(result.length() - 1);

    return result;
}

} // namespace utils

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE void
ui_open_callback(void* output_is_set, void* output, const char* fileName, int last)
{
    if (fileName) {
        // Append filename to result
        (*reinterpret_cast<std::vector<std::string>*>(output)).push_back(fileName);
    }

    // If last, indicate that output is set
    if (last != 0) {
        (*reinterpret_cast<bool*>(output_is_set)) = true;
    }
}
extern "C" EMSCRIPTEN_KEEPALIVE void
ui_save_callback(void* output_is_set, void* output, const char* fileName)
{
    if (fileName) {
        (*reinterpret_cast<std::string*>(output)) = fileName;
    }

    (*reinterpret_cast<bool*>(output_is_set)) = true;
}


std::vector<std::string> open_file_js(const std::vector<FileFilter>& filters, bool multiple = false)
{
    const auto accepted_formats = utils::transform_filters_to_accept(filters);
    std::vector<std::string> selection;
    bool result_is_set = false;
    int multiple_flag = multiple ? 1 : 0;

    // clang-format off
    EM_ASM_(
        {
            var felem = document.createElement("input");
            felem.setAttribute("type", "file");
            felem.setAttribute("hidden", "true");
            felem.setAttribute("accept", UTF8ToString($0));
            if ($3 == 1) {
                felem.setAttribute("multiple", "");
            }

            let focus_listener = function(e)
            {
                window.removeEventListener('focus', focus_listener);
                // Use delay because input element value changes after focus
                setTimeout(
                    function() {
                        if (felem.value.length == 0) {
                            // Remove file element
                            felem.remove();

                            // Send cancel signal to C++ code
                            ccall(
                                "ui_open_callback",
                                null,
                                ["number", "number", "string", "number"],
                                [$1, $2, 0, 1]);
                        }
                    },
                    1000);
            };

            // Cancellation handle
            felem.onclick = function(event) { window.addEventListener('focus', focus_listener); };

            felem.onchange = function(event)
            {
                for (var i = 0; i < event.target.files.length; i++) {
                    const file = event.target.files[i];
                    var reader = new FileReader();
                    reader.onload = (function(index, reader) {
                        return function()
                        {
                            let name = event.target.files[index].name;
                            var bytes = new Uint8Array(reader.result);

                            console.debug("Opening file " + name);

                            //If opening multiple files, make sure they're unique by prepending
                            // an index, so that extension remains unchanged
                            if(event.target.files.length > 1){
                               name = index.toString() + "_" + name; 
                            }
                            
                            FS.writeFile(name, bytes);
                            const is_last = (index == (event.target.files.length - 1)) ? 1 : 0;
                            ccall(
                                "ui_open_callback",
                                null,
                                ["number", "number", "string", "number"],
                                [$1, $2, name, is_last]);
                        };
                    })(i, reader);

                    reader.addEventListener('error', () => {
                            // Call callback without filename to signify error
                            ccall(
                                "ui_open_callback",
                                null,
                                ["number", "number", "string", "number"],
                                [$1, $2, null, 1]);
                        });
                    reader.readAsArrayBuffer(file);
                }

                felem.remove();
            };

            document.body.appendChild(felem);
            felem.click();
        },

        accepted_formats.c_str(),
        &result_is_set,
        &selection,
        multiple_flag);

    // clang-format on

    while (true) {
        if (result_is_set) {
            break;
        }
        emscripten_sleep(10);
    }

    return selection;
}
#endif


FileDialogPath open_file(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters)
{
#if defined(__EMSCRIPTEN__)
    (void)(title);
    (void)(default_path);
    auto selection = open_file_js(filters);
#else
    auto selection =
        pfd::open_file(title, default_path.string(), utils::transform_filters(filters)).result();
#endif

    if (selection.size() > 2) {
        throw std::runtime_error(
            "Cannot select > 1 file without the multiselect flag. Please report this as a bug");
    }
    if (selection.empty()) {
        return FileDialogPath::make_input_path({});
    } else {
        return FileDialogPath::make_input_path(selection.front());
    }
}

std::vector<FileDialogPath> open_files(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters)
{
#if defined(__EMSCRIPTEN__)
    (void)(title);
    (void)(default_path);
    auto selection = open_file_js(filters, true);

#else
    auto selection = pfd::open_file(
                         title,
                         default_path.string(),
                         utils::transform_filters(filters),
                         pfd::opt::multiselect)
                         .result();
#endif
    std::vector<FileDialogPath> results;
    for (auto& path : selection) {
        results.push_back(FileDialogPath::make_input_path(path));
    }
    return results;
}

FileDialogPath save_file(
    const std::string& title,
    const fs::path& default_path,
    const std::vector<FileFilter>& filters,
    FileSave overwrite_behavior)
{
#if defined(__EMSCRIPTEN__)

    (void)(default_path);
    (void)(filters);
    (void)(overwrite_behavior);

    std::string result;
    bool result_is_set;
    EM_ASM_(
        {
            // Ask use for filename
            console.debug("Opening file name prompt.");
            let filename = prompt(UTF8ToString($0), "");
            if (filename == null) {
                filename = "";
            }
            ccall("ui_save_callback", null, ["number", "number", "string"], [$1, $2, filename]);
        },
        title.c_str(),
        &result_is_set,
        &result);

    // Wait until result is set
    while (true) {
        if (result_is_set) {
            break;
        }
        emscripten_sleep(10);
    }

    return FileDialogPath::make_output_path(result);

#else

    pfd::opt option =
        (overwrite_behavior == FileSave::SilentOverwrite ? pfd::opt::force_overwrite
                                                         : pfd::opt::none);
    auto path =
        pfd::save_file(title, default_path.string(), utils::transform_filters(filters), option)
            .result();

    return FileDialogPath::make_output_path(path);
#endif
}

FileDialogPath
open_folder(const std::string& title, const fs::path& default_path, FolderOpen open_behavior)
{
    pfd::opt option =
        (open_behavior == FolderOpen::LastOpened ? pfd::opt::force_path : pfd::opt::none);

    auto path = pfd::select_folder(title, default_path.string(), option).result();
    return FileDialogPath::make_input_path(path);
}


} // namespace ui
} // namespace lagrange
