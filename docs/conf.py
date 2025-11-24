# Configuration file for the Sphinx documentation builder.

import subprocess
import os
import re

# Project information
project = 'libaeon'
copyright = '2006-2025, Elden Armbrust'
author = 'Elden Armbrust'

# Get version from git tags, fall back to branch name
def get_version():
    """Get version from git tags, fallback to branch name"""
    try:
        # Get git describe output
        git_describe = subprocess.check_output(
            ['git', 'describe', '--tags', '--always'],
            cwd=os.path.dirname(__file__),
            stderr=subprocess.DEVNULL,
            text=True
        ).strip()
        
        # Parse version from git describe output
        # Format can be: 0.16.22-79-g184ecca or v1.18.0 or commit hash
        match = re.match(r'^(v)?(\d+\.\d+\.\d+)', git_describe)
        if match:
            # Extract clean version number
            version = match.group(2)
            print(f"Version from git describe: {git_describe}")
            print(f"Parsed version: {version}")
            return version
        else:
            # Not a version tag, fall through to branch name
            raise ValueError("Not a version tag format")
    except (subprocess.CalledProcessError, ValueError):
        try:
            # Fall back to branch name
            branch = subprocess.check_output(
                ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                cwd=os.path.dirname(__file__),
                stderr=subprocess.DEVNULL,
                text=True
            ).strip()
            if branch and branch != 'HEAD':
                print(f"Version from git branch: {branch}")
                return branch
            else:
                raise ValueError("No branch name")
        except (subprocess.CalledProcessError, ValueError):
            # Not in a git repo or git not available
            print("Not in git repo, using default version")
            return 'dev'

# Get release version
release = get_version()
# Extract major.minor for version
version_parts = release.split('.')
version = '.'.join(version_parts[:2]) if len(version_parts) >= 2 else release

print(f"Documentation version: {version}")
print(f"Release: {release}")

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

print(f"\nBreathе configuration:")
print(f"  Project: 'libaeon'")
print(f"  XML directory: {xml_dir}")
print(f"  Path exists: {os.path.exists(xml_dir)}")
if os.path.exists(xml_dir):
    xml_files = [f for f in os.listdir(xml_dir) if f.endswith('.xml')]
    print(f"  XML files found: {len(xml_files)}")