#!/bin/bash

# Tape Death - Create Distributable Installer Package
# Creates a DMG containing the plugins and an installer script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="Tape Death"
VERSION="1.0.0"
DMG_NAME="TapeDeath-${VERSION}-macOS"

# Plugin source locations
AU_SOURCE="$SCRIPT_DIR/build/TapeDeath_artefacts/AU/${PLUGIN_NAME}.component"
VST3_SOURCE="$SCRIPT_DIR/build/TapeDeath_artefacts/VST3/${PLUGIN_NAME}.vst3"

# Temp directory for packaging
PACKAGE_DIR="$SCRIPT_DIR/installer_package"
DMG_CONTENTS="$PACKAGE_DIR/dmg_contents"

echo "========================================"
echo "  Creating Tape Death Installer"
echo "========================================"
echo ""

# Check if plugins exist
if [ ! -d "$AU_SOURCE" ] || [ ! -d "$VST3_SOURCE" ]; then
    echo "Error: Plugins not found. Building first..."
    cd "$SCRIPT_DIR/build"
    cmake --build .
    cd "$SCRIPT_DIR"
fi

# Clean up previous package
rm -rf "$PACKAGE_DIR"
mkdir -p "$DMG_CONTENTS"

# Create Plugins directory in package
mkdir -p "$DMG_CONTENTS/Plugins/AU"
mkdir -p "$DMG_CONTENTS/Plugins/VST3"

# Copy plugins
echo "Copying plugins..."
cp -R "$AU_SOURCE" "$DMG_CONTENTS/Plugins/AU/"
cp -R "$VST3_SOURCE" "$DMG_CONTENTS/Plugins/VST3/"

# Create the installer script for the DMG
cat > "$DMG_CONTENTS/Install Tape Death.command" << 'INSTALLER_SCRIPT'
#!/bin/bash

# Tape Death Plugin Installer
# Double-click this file to install the plugins

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="Tape Death"

AU_SOURCE="$SCRIPT_DIR/Plugins/AU/${PLUGIN_NAME}.component"
VST3_SOURCE="$SCRIPT_DIR/Plugins/VST3/${PLUGIN_NAME}.vst3"

AU_DEST="/Library/Audio/Plug-Ins/Components"
VST3_DEST="/Library/Audio/Plug-Ins/VST3"

clear
echo "========================================"
echo "  Tape Death Plugin Installer"
echo "========================================"
echo ""
echo "This will install:"
echo "  - AU plugin  -> $AU_DEST"
echo "  - VST3 plugin -> $VST3_DEST"
echo ""
echo "Administrator privileges required."
echo ""

# Check if plugins exist in package
if [ ! -d "$AU_SOURCE" ] || [ ! -d "$VST3_SOURCE" ]; then
    echo "Error: Plugin files not found in package."
    echo "Press any key to exit..."
    read -n 1
    exit 1
fi

# Create directories
sudo mkdir -p "$AU_DEST"
sudo mkdir -p "$VST3_DEST"

# Remove old versions
sudo rm -rf "$AU_DEST/${PLUGIN_NAME}.component" 2>/dev/null || true
sudo rm -rf "$VST3_DEST/${PLUGIN_NAME}.vst3" 2>/dev/null || true

# Install
echo "Installing AU plugin..."
sudo cp -R "$AU_SOURCE" "$AU_DEST/"

echo "Installing VST3 plugin..."
sudo cp -R "$VST3_SOURCE" "$VST3_DEST/"

# Set permissions
echo "Setting permissions..."
sudo chmod -R 755 "$AU_DEST/${PLUGIN_NAME}.component"
sudo chmod -R 755 "$VST3_DEST/${PLUGIN_NAME}.vst3"
sudo chown -R root:wheel "$AU_DEST/${PLUGIN_NAME}.component"
sudo chown -R root:wheel "$VST3_DEST/${PLUGIN_NAME}.vst3"

# Remove quarantine
sudo xattr -rd com.apple.quarantine "$AU_DEST/${PLUGIN_NAME}.component" 2>/dev/null || true
sudo xattr -rd com.apple.quarantine "$VST3_DEST/${PLUGIN_NAME}.vst3" 2>/dev/null || true

echo ""
echo "========================================"
echo "  Installation Complete!"
echo "========================================"
echo ""
echo "Please restart your DAW to load the plugins."
echo ""
echo "Press any key to exit..."
read -n 1
INSTALLER_SCRIPT

chmod +x "$DMG_CONTENTS/Install Tape Death.command"

# Create README
cat > "$DMG_CONTENTS/README.txt" << 'README'
Tape Death - Audio Plugin
=========================

A tape degradation effect plugin for macOS.

INSTALLATION
------------
Double-click "Install Tape Death.command" and enter your
administrator password when prompted.

The installer will copy the plugins to:
  - AU:   /Library/Audio/Plug-Ins/Components/
  - VST3: /Library/Audio/Plug-Ins/VST3/

After installation, restart your DAW to load the plugins.

MANUAL INSTALLATION
-------------------
If the installer doesn't work, you can manually copy:
  - Plugins/AU/Tape Death.component -> /Library/Audio/Plug-Ins/Components/
  - Plugins/VST3/Tape Death.vst3   -> /Library/Audio/Plug-Ins/VST3/

UNINSTALLATION
--------------
To remove the plugins, delete these files:
  - /Library/Audio/Plug-Ins/Components/Tape Death.component
  - /Library/Audio/Plug-Ins/VST3/Tape Death.vst3

SUPPORTED FORMATS
-----------------
  - AU (Audio Unit) - for Logic Pro, GarageBand, etc.
  - VST3 - for Ableton Live, FL Studio, Cubase, etc.

README

# Create the DMG
echo "Creating DMG..."
DMG_PATH="$SCRIPT_DIR/$DMG_NAME.dmg"
rm -f "$DMG_PATH"

hdiutil create -volname "$PLUGIN_NAME" \
    -srcfolder "$DMG_CONTENTS" \
    -ov -format UDZO \
    "$DMG_PATH"

# Clean up
rm -rf "$PACKAGE_DIR"

echo ""
echo "========================================"
echo "  Installer Created!"
echo "========================================"
echo ""
echo "DMG location: $DMG_PATH"
echo ""
echo "Distribute this DMG to users. They can:"
echo "  1. Double-click the DMG to mount it"
echo "  2. Double-click 'Install Tape Death.command'"
echo "  3. Enter their admin password"
echo ""
