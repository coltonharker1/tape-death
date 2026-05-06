# Tape Death

A tape degradation audio effect plugin for macOS. Simulates the character and decay of worn, damaged, and ancient tape — saturation, dropouts, hiss, crackle, wow/flutter, and the slow filter wandering of magnetic tape that has lived a long life.

VST3 and Audio Unit, stereo in / stereo out.

## Controls

**Degradation**
- **Damage** — master throttle for dropout amount (0–100%)
- **Rot** — dropout severity: how deep each dropout cuts and how long it can stick
- **Darkness** — tone control / high-frequency loss (20kHz → 500Hz)
- **Violence** — speed and aggression of dropouts (smooth fade ↔ snappy chops)

**Character**
- **Saturation** — asymmetric warm-tube + tanh stage, 2× oversampled
- **Warble** — wow & flutter, fractional-delay pitch modulation with cubic Hermite interpolation
- **Noise** — pink-noise hiss + bandlimited crackle pops (separate filter paths)
- **Age** — slow filter wandering and pitch drift, multiple sub-Hz LFOs

**Output**
- **Lo-Cut** — 12 dB/oct high-pass (20 Hz – 2 kHz)
- **Hi-Cut** — 24 dB/oct low-pass (500 Hz – 20 kHz)
- **Dry/Wet** — smoothed mix

## Installation

The plugin is **not signed or notarized**. macOS Gatekeeper will flag it. The steps below get you around it.

### Option 1 — DMG installer (recommended)

1. Download the latest `TapeDeath-x.y.z-macOS.dmg` from [Releases](https://github.com/coltonharker1/tape-death/releases).
2. Double-click the DMG to mount it.
3. **Right-click** (or Control-click) `Install Tape Death.command` → **Open**. Confirm in the dialog.
   - If you double-click instead, macOS will refuse with *"Apple cannot check it for malicious software"*. Right-click → Open is the one-time bypass.
4. Enter your admin password when prompted.
5. Restart your DAW.

The installer copies the plugins, sets correct ownership/permissions, and strips the `com.apple.quarantine` attribute so the DAW will load them.

### Option 2 — Manual install

If the installer fails, copy the bundles by hand and remove quarantine yourself:

```bash
# 1. Copy
sudo cp -R "Tape Death.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "Tape Death.vst3"      /Library/Audio/Plug-Ins/VST3/

# 2. Permissions + ownership
sudo chmod -R 755 "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo chmod -R 755 "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
sudo chown -R root:wheel "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo chown -R root:wheel "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"

# 3. Strip Gatekeeper quarantine (the critical step)
sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
```

### Troubleshooting Gatekeeper

If your DAW silently refuses to load the plugin, or you see *"Tape Death.component is damaged and can't be opened"*:

```bash
# Nuke all quarantine / extended attrs and re-bless the bundle
sudo xattr -cr "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
```

If macOS still refuses, open **System Settings → Privacy & Security**, scroll to the bottom, and click **Allow Anyway** next to the Tape Death entry that appears after the first load attempt.

### Supported hosts

- **AU**: Logic Pro, GarageBand, MainStage
- **VST3**: Ableton Live, FL Studio, Cubase, Reaper, Bitwig, Studio One, etc.

## Building from source

### Requirements

- macOS 10.13 or later
- Xcode Command Line Tools
- CMake 3.22+
- [JUCE](https://juce.com/) (clone separately)

### Build

```bash
git clone https://github.com/coltonharker1/tape-death.git
cd tape-death
```

Update the JUCE path in `CMakeLists.txt` (line 12) to point at your JUCE checkout, then:

```bash
cmake -B build -S .
cmake --build build --config Release -j
```

Built bundles end up in:

- `build/TapeDeath_artefacts/AU/Tape Death.component`
- `build/TapeDeath_artefacts/VST3/Tape Death.vst3`

`COPY_PLUGIN_AFTER_BUILD` is on, so a Release build also copies the freshly-built bundles into `~/Library/Audio/Plug-Ins/`. No quarantine is set on locally-built bundles, so they load directly — no `xattr` dance needed for self-builds.

### Packaging an installer

```bash
./create_installer.sh
```

Produces `TapeDeath-1.0.0-macOS.dmg` containing both bundles + the install script.

## Uninstall

```bash
sudo rm -rf "/Library/Audio/Plug-Ins/Components/Tape Death.component"
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Tape Death.vst3"
```

## License

MIT — see `LICENSE`.

## Credits

Developed by Project Sonaris.
