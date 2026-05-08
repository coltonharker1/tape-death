# CMake configuration for Tape Death Plugin
cmake_minimum_required(VERSION 3.22)

# Project configuration
project(TapeDeath VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(/Users/colton/code/me/plugins/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Add sonorous-kit. Sibling clone preferred for active development across all
# Project Sonorous repos; falls back to fetching v0.2.0 from GitHub for casual
# clones so `cmake -B build` works out of the box.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../sonorous-kit/CMakeLists.txt)
    message(STATUS "Tape Death: using sibling sonorous-kit")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../sonorous-kit
                     ${CMAKE_CURRENT_BINARY_DIR}/sonorous-kit)
else()
    message(STATUS "Tape Death: fetching sonorous-kit from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        sonorous_kit
        GIT_REPOSITORY https://github.com/coltonharker1/sonorous-kit.git
        GIT_TAG        v0.2.0)
    FetchContent_MakeAvailable(sonorous_kit)
endif()

# Plugin configuration
juce_add_plugin(TapeDeath
    COMPANY_NAME "Project Sonorous"
    BUNDLE_ID "com.projectsonorous.tapedeath"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE PjSo
    PLUGIN_CODE TpDh
    FORMATS VST3 AU
    PRODUCT_NAME "Tape Death")

# Generate JuceHeader.h
juce_generate_juce_header(TapeDeath)

# The three Sonorous typefaces (Instrument Serif x2, JetBrains Mono) now come
# from sonorous_visual. The XXII Ultimate font and the wordmark PNG that lived
# here previously aren't referenced anywhere in the source — files remain in
# Source/Fonts/ and Source/Images/ for now, but no longer participate in the
# build. Re-add a juce_add_binary_data target if Tape Death ever needs its own
# instrument-specific binary assets again.

# Source files
target_sources(TapeDeath
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/TapeDeathEngine.h)

# Link JUCE modules and the Sonorous visual kit
target_link_libraries(TapeDeath
    PRIVATE
        sonorous_visual
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
