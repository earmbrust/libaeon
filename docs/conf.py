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

# Setup paths
docs_dir = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.dirname(docs_dir)
xml_dir = os.path.join(docs_dir, '_build', 'doxygen', 'xml')
doxyfile_path = os.path.join(root_dir, 'Doxyfile')

def run_doxygen_if_needed():
    """Run Doxygen if XML output doesn't exist"""
    index_xml = os.path.join(xml_dir, 'index.xml')
    
    if os.path.exists(index_xml):
        print(f"✓ Doxygen XML already exists at {xml_dir}")
        return
    
    print(f"Running Doxygen to generate XML...")
    if not os.path.exists(doxyfile_path):
        raise FileNotFoundError(f"Doxyfile not found: {doxyfile_path}")
    
    result = subprocess.call(['doxygen', doxyfile_path], cwd=root_dir)
    if result != 0:
        raise RuntimeError(f"Doxygen failed with exit code {result}")
    
    if not os.path.exists(index_xml):
        raise FileNotFoundError(f"Doxygen XML not generated at {xml_dir}")
    
    print(f"✓ Doxygen XML generated successfully")

# Run Doxygen before Breathe configuration
run_doxygen_if_needed()

# Breathe configuration - maps project name to XML directory
# Use os.fspath() to satisfy Sphinx 8 requirements
breathe_projects = {
    'libaeon': os.fspath(xml_dir)
}

breathe_default_project = 'libaeon'

# Optional: if using auto directives, enable member and undocumented members
breathe_show_define_initializer = True

print(f"\nBreathе configuration:")
print(f"  Project: 'libaeon'")
print(f"  XML directory: {xml_dir}")
print(f"  Path exists: {os.path.exists(xml_dir)}")
if os.path.exists(xml_dir):
    xml_files = [f for f in os.listdir(xml_dir) if f.endswith('.xml')]
    print(f"  XML files found: {len(xml_files)}")