# CMake configuration for Tape Death Plugin
cmake_minimum_required(VERSION 3.22)

# Project configuration
project(TapeDeath VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(/Users/colton/code/me/plugins/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Plugin configuration
juce_add_plugin(TapeDeath
    COMPANY_NAME "Project Sonaris"
    BUNDLE_ID "com.projectsonaris.tapedeath"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE PjSn
    PLUGIN_CODE TpDh
    FORMATS VST3 AU
    PRODUCT_NAME "Tape Death")

# Generate JuceHeader.h
juce_generate_juce_header(TapeDeath)

# Binary data (fonts and images)
juce_add_binary_data(TapeDeathBinaryData
    SOURCES
        Source/Fonts/XXII\ Ultimate-Black-Metal.ttf
        Source/Fonts/InstrumentSerif-Regular.ttf
        Source/Fonts/InstrumentSerif-Italic.ttf
        Source/Fonts/JetBrainsMono-Regular.ttf
        Source/Images/tape\ death\ 2.png)

# Source files
target_sources(TapeDeath
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/TapeDeathEngine.h)

# Link JUCE modules
target_link_libraries(TapeDeath
    PRIVATE
        TapeDeathBinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Compiler definitions
target_compile_definitions(TapeDeath
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Performance optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(TapeDeath PRIVATE
        -O3
        -funroll-loops
        -fvectorize
        -march=native
        -fno-math-errno
    )
elseif(MSVC)
    target_compile_options(TapeDeath PRIVATE
        /O2
        /arch:AVX2
    )
endif()
