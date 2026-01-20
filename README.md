# Tape Death

A tape degradation audio effect plugin for macOS. Simulates the character and destruction of worn, damaged, and ancient tape recordings.

## Features

**Degradation Controls:**
- **Damage** - Overall degradation intensity
- **Rot** - Dropout density and depth
- **Darkness** - High-frequency loss (tone darkening)
- **Violence** - Dropout harshness and attack speed

**Character Controls:**
- **Saturation** - Tape saturation/warmth
- **Warble** - Wow and flutter pitch modulation
- **Noise** - Tape hiss and crackle
- **Age** - Filter wandering and pitch drift

**Output Controls:**
- **Lo-Cut** - High-pass filter (20Hz - 2kHz)
- **Hi-Cut** - Low-pass filter (500Hz - 20kHz)
- **Dry/Wet** - Effect blend

## Supported Formats

- **AU** (Audio Unit) - Logic Pro, GarageBand, etc.
- **VST3** - Ableton Live, FL Studio, Cubase, Reaper, etc.

## Installation

### Option 1: Download the Installer (Recommended)

1. Download the latest DMG from [Releases](https://github.com/coltonharker1/tape-death/releases)
2. Double-click the DMG to mount it
3. Double-click **"Install Tape Death.command"**
4. Enter your administrator password when prompted
5. Restart your DAW

### Option 2: Manual Installation

Copy the plugins to your system plugin folders:

```bash
# AU Plugin
sudo cp -R "Tape Death.component" "/Library/Audio/Plug-Ins/Components/"

# VST3 Plugin
sudo cp -R "Tape Death.vst3" "/Library/Audio/Plug-Ins/VST3/"

# Set permissions
sudo chmod -R 755 "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo chmod -R 755 "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
```

## Building from Source

### Requirements

- macOS 10.13 or later
- CMake 3.22 or later
- Xcode Command Line Tools
- [JUCE Framework](https://juce.com/) (clone to a known location)

### Build Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/coltonharker1/tape-death.git
   cd tape-death
   ```

2. Update the JUCE path in `CMakeLists.txt` (line 12) to point to your JUCE installation:
   ```cmake
   add_subdirectory(/path/to/your/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
   ```

3. Build the plugin:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

4. The built plugins will be in:
   - `build/TapeDeath_artefacts/AU/Tape Death.component`
   - `build/TapeDeath_artefacts/VST3/Tape Death.vst3`

### Creating an Installer

After building, run the installer script:

```bash
./create_installer.sh
```

This creates `TapeDeath-1.0.0-macOS.dmg` for distribution.

## Uninstallation

Delete these files:
```bash
sudo rm -rf "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
```

## License

MIT License - See LICENSE file for details.

## Credits

Developed by Project Sonaris
