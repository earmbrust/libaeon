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
breathe_projects = {
    'libaeon': os.path.join(os.path.dirname(__file__), '_build', 'doxygen', 'xml')
}
breathe_default_project = 'libaeon'

# Run Doxygen before building Sphinx if XML doesn't exist
# This is necessary for Read the Docs (GitHub Actions runs it separately via workflow)
def run_doxygen(app, config):
    """Run Doxygen before building Sphinx documentation if XML doesn't exist"""
    docs_dir = os.path.dirname(__file__)
    root_dir = os.path.dirname(docs_dir)
    xml_dir = os.path.join(docs_dir, '_build', 'doxygen', 'xml')
    doxyfile = os.path.join(root_dir, 'Doxyfile')
    
    # Only run if XML doesn't exist (GitHub Actions already generates it)
    if os.path.exists(xml_dir) and os.path.exists(os.path.join(xml_dir, 'index.xml')):
        print("✓ Doxygen XML already exists, skipping Doxygen build")
        return
    
    if not os.path.exists(doxyfile):
        raise FileNotFoundError(f"Doxyfile not found at {doxyfile}")
    
    print("=" * 60)
    print("Running Doxygen...")
    print("=" * 60)
    print(f"Found Doxyfile at: {doxyfile}")
    print(f"Running from directory: {root_dir}")
    
    result = subprocess.call(['doxygen', doxyfile], cwd=root_dir)
    
    if result == 0:
        print("✓ Doxygen completed successfully")
    else:
        print(f"✗ Warning: Doxygen exited with code {result}")
    
    # Verify XML was generated
    if os.path.exists(xml_dir):
        xml_files = [f for f in os.listdir(xml_dir) if f.endswith('.xml')]
        print(f"✓ Doxygen XML generated: {len(xml_files)} XML files")
        
        if not os.path.exists(os.path.join(xml_dir, 'index.xml')):
            raise FileNotFoundError(f"Critical: index.xml not found at {xml_dir}")
    else:
        raise FileNotFoundError(f"Doxygen XML output not found at {xml_dir}")
    
    print("=" * 60)
    print()

def setup(app):
    app.connect('config-inited', run_doxygen)