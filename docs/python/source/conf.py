# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import re
import lagrange

from typing import Any
from sphinx.application import Sphinx

project = "Lagrange"
copyright = "2023, Adobe"
author = "Lagrange Core Team"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx_autodoc_typehints",
    "autoapi.extension",
]

# Where the Lagrange python package is installed
package_dir = next(x for x in lagrange.__path__ if "site-packages" in x)
autoapi_dirs = [f"{package_dir}"]

autoapi_ignore = ["*/_version.py", "*/_logging.py"]

# https://github.com/readthedocs/sphinx-autoapi/issues/285
suppress_warnings = ["autoapi.python_import_resolution"]

autoapi_member_order = "groupwise"
# autoapi_generate_api_docs = False

autoapi_template_dir = "_autoapi"

autosummary_generate = True

templates_path = ["_templates"]
exclude_patterns = ["_autoapi"]

# Auto typehints settings
autodoc_typehints = "description"
always_use_bars_union = True

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "furo"
html_static_path = ["_static"]

html_show_sourcelink = False

# -- Custom functions for TOC prefix stripping -------------------------------


def strip_package_prefix_from_html(html: str, package_name: str = "lagrange") -> str:
    """
    Remove leading 'package_name.' or 'package_name_' from link text in an HTML fragment.

    Args:
        html: HTML string to process.
        package_name: Package name prefix to remove.

    Returns:
        Modified HTML with package prefix removed from link text.
    """
    if not html:
        return html

    # Pattern matches: <a ...>lagrange. or <a ...>lagrange_
    # and removes the prefix from the link text
    pattern = re.compile(r"(<a[^>]*>)(?:" + re.escape(package_name) + r"[\._])")
    return pattern.sub(r"\1", html)


def strip_package_prefix_from_toc(
    app: Sphinx, pagename: str, templatename: str, context: dict, doctree
) -> None:
    """
    Sphinx event handler to post-process context HTML fragments.

    Removes package prefix from TOC entries for cleaner display.

    Args:
        app: Sphinx application instance.
        pagename: Name of the page being rendered.
        templatename: Template being used.
        context: Template context dictionary.
        doctree: Document tree (unused).
    """
    package_name = "lagrange"

    # Process various context keys that may contain TOC HTML
    for key in ("toc", "body"):
        val = context.get(key)
        if isinstance(val, str):
            context[key] = strip_package_prefix_from_html(val, package_name)


def setup(app: Sphinx) -> dict[str, Any]:
    """
    Sphinx extension setup function.

    Connects event handlers to strip package prefix from doctree and TOC.

    Args:
        app: Sphinx application instance.

    Returns:
        Extension metadata dictionary.
    """
    app.connect("html-page-context", strip_package_prefix_from_toc)

    return {
        "version": "0.1",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
