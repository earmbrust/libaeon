# Configuration file for the Sphinx documentation builder.
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import subprocess
import os

# Project information
project = 'libaeon'
copyright = '2006-2025, Elden Armbrust'
author = 'Elden Armbrust'
release = '0.18.3'
version = '0.18'

# General configuration
extensions = [
    'breathe',
    'sphinx_rtd_theme',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

language = 'en'

# Read the Docs theme
html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

html_theme_options = {
    'logo_only': False,
    'display_version': True,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    'vcs_pageview_mode': '',
    'style_nav_header_background': '#2980B9',
    # Toc options
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False,
}

html_logo = None
html_title = 'libaeon - Networking Library'

# Breathe configuration
breathe_projects = {
    'libaeon': './_build/doxygen/xml'
}
breathe_default_project = 'libaeon'

# Run Doxygen before building Sphinx
def run_doxygen(app, config):
    """Run Doxygen before building Sphinx documentation"""
    print("Running Doxygen...")
    doxygen_dir = os.path.dirname(__file__)
    doxyfile = os.path.join(os.path.dirname(doxygen_dir), 'Doxyfile')
    
    if os.path.exists(doxyfile):
        subprocess.call(['doxygen', doxyfile], cwd=os.path.dirname(doxygen_dir))
    else:
        print(f"Warning: Doxyfile not found at {doxyfile}")

def setup(app):
    app.connect('config-inited', run_doxygen)
