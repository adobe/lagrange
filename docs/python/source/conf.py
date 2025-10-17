# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import os
import pathlib
import lagrange

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

# https://github.com/readthedocs/sphinx-autoapi/issues/405
autoapi_ignore = ["*__init__.py*"]

autoapi_member_order = "groupwise"
# autoapi_generate_api_docs = False

autosummary_generate = True

templates_path = ["_templates"]
exclude_patterns = []

# Auto typehints settings
autodoc_typehints = "description"
always_use_bars_union = True

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "furo"
html_static_path = ["_static"]
