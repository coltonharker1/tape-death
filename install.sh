#!/bin/bash

# Tape Death Plugin Installer
# Installs AU and VST3 formats to system plugin directories

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="Tape Death"

# Plugin source locations (from build directory)
AU_SOURCE="$SCRIPT_DIR/build/TapeDeath_artefacts/AU/${PLUGIN_NAME}.component"
VST3_SOURCE="$SCRIPT_DIR/build/TapeDeath_artefacts/VST3/${PLUGIN_NAME}.vst3"

# System plugin directories
AU_DEST="/Library/Audio/Plug-Ins/Components"
VST3_DEST="/Library/Audio/Plug-Ins/VST3"

echo "========================================"
echo "  Tape Death Plugin Installer"
echo "========================================"
echo ""

# Check if plugins exist
if [ ! -d "$AU_SOURCE" ]; then
    echo "Error: AU plugin not found at $AU_SOURCE"
    echo "Please build the plugin first: cd build && cmake .. && cmake --build ."
    exit 1
fi

if [ ! -d "$VST3_SOURCE" ]; then
    echo "Error: VST3 plugin not found at $VST3_SOURCE"
    echo "Please build the plugin first: cd build && cmake .. && cmake --build ."
    exit 1
fi

echo "Found plugins:"
echo "  - AU:   $AU_SOURCE"
echo "  - VST3: $VST3_SOURCE"
echo ""

# Check for sudo access
echo "Installing plugins requires administrator privileges."
echo ""

# Create destination directories if they don't exist
sudo mkdir -p "$AU_DEST"
sudo mkdir -p "$VST3_DEST"

# Remove old versions if they exist
if [ -d "$AU_DEST/${PLUGIN_NAME}.component" ]; then
    echo "Removing existing AU plugin..."
    sudo rm -rf "$AU_DEST/${PLUGIN_NAME}.component"
fi

if [ -d "$VST3_DEST/${PLUGIN_NAME}.vst3" ]; then
    echo "Removing existing VST3 plugin..."
    sudo rm -rf "$VST3_DEST/${PLUGIN_NAME}.vst3"
fi

# Copy plugins
echo "Installing AU plugin..."
sudo cp -R "$AU_SOURCE" "$AU_DEST/"

echo "Installing VST3 plugin..."
sudo cp -R "$VST3_SOURCE" "$VST3_DEST/"

# Set permissions (readable and executable by all users)
echo "Setting permissions..."
sudo chmod -R 755 "$AU_DEST/${PLUGIN_NAME}.component"
sudo chmod -R 755 "$VST3_DEST/${PLUGIN_NAME}.vst3"

# Set ownership to root:wheel (standard for system plugins)
sudo chown -R root:wheel "$AU_DEST/${PLUGIN_NAME}.component"
sudo chown -R root:wheel "$VST3_DEST/${PLUGIN_NAME}.vst3"

# Remove quarantine attribute (in case of download)
echo "Removing quarantine attributes..."
sudo xattr -rd com.apple.quarantine "$AU_DEST/${PLUGIN_NAME}.component" 2>/dev/null || true
sudo xattr -rd com.apple.quarantine "$VST3_DEST/${PLUGIN_NAME}.vst3" 2>/dev/null || true

echo ""
echo "========================================"
echo "  Installation Complete!"
echo "========================================"
echo ""
echo "Installed to:"
echo "  - AU:   $AU_DEST/${PLUGIN_NAME}.component"
echo "  - VST3: $VST3_DEST/${PLUGIN_NAME}.vst3"
echo ""
echo "Please restart your DAW to load the new plugins."
echo ""
