# Configuration file for the Sphinx documentation builder.
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import subprocess
import os

# Project information
project = 'libaeon'
copyright = '2006-2025, Elden Armbrust'
author = 'Elden Armbrust'
release = '1.18.0'
version = '1.18'

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
    'style_nav_header_background': '#2980B9',
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False,
}

html_title = 'libaeon - Networking Library'

# Breathe configuration
# The Doxyfile specifies OUTPUT_DIRECTORY = ./docs/_build/doxygen
# and XML_OUTPUT = xml
# So the XML will be at ./docs/_build/doxygen/xml
breathe_projects = {
    'libaeon': os.path.join(os.path.dirname(__file__), '_build', 'doxygen', 'xml')
}
breathe_default_project = 'libaeon'

# Run Doxygen before building Sphinx
def run_doxygen(app, config):
    """Run Doxygen before building Sphinx documentation"""
    print("=" * 60)
    print("Running Doxygen...")
    print("=" * 60)
    docs_dir = os.path.dirname(__file__)
    root_dir = os.path.dirname(docs_dir)
    doxyfile = os.path.join(root_dir, 'Doxyfile')
    
    if os.path.exists(doxyfile):
        print(f"Found Doxyfile at: {doxyfile}")
        print(f"Running from directory: {root_dir}")
        result = subprocess.call(['doxygen', doxyfile], cwd=root_dir)
        
        if result == 0:
            print("✓ Doxygen completed successfully")
        else:
            print(f"✗ Warning: Doxygen exited with code {result}")
        
        # Verify XML was generated
        xml_dir = os.path.join(docs_dir, '_build', 'doxygen', 'xml')
        if os.path.exists(xml_dir):
            print(f"✓ Doxygen XML generated at: {xml_dir}")
            # Count generated files
            xml_files = [f for f in os.listdir(xml_dir) if f.endswith('.xml')]
            print(f"  Generated {len(xml_files)} XML files")
        else:
            print(f"✗ ERROR: Doxygen XML not found at: {xml_dir}")
            print(f"  Checked: {xml_dir}")
            raise FileNotFoundError(f"Doxygen XML output not found at {xml_dir}")
    else:
        raise FileNotFoundError(f"Doxyfile not found at {doxyfile}")
    
    print("=" * 60)
    print()

def setup(app):
    app.connect('config-inited', run_doxygen)
