cmake_minimum_required(VERSION 3.22)
project(Miditin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

# IBM Plex Mono and IBM Plex Sans Condensed: SIL OFL 1.1 with Reserved Font Name "Plex". Embedded
# byte-for-byte as IBM ships them -- subsetting or otherwise modifying a file would make it a
# derivative that may not carry the name. Resources/Fonts/OFL-IBMPlex.txt ships with any binary.
juce_add_binary_data(MiditinData
    SOURCES
        Resources/Fonts/IBMPlexMono-Regular.ttf
        Resources/Fonts/IBMPlexMono-Medium.ttf
        Resources/Fonts/IBMPlexMono-SemiBold.ttf
        Resources/Fonts/IBMPlexSansCondensed-Medium.ttf
        Resources/Fonts/IBMPlexSansCondensed-SemiBold.ttf)
set_target_properties(MiditinData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Miditin
    COMPANY_NAME "Autotel"
    # VST3 has no MIDI-effect category. An instrument with MIDI out is what REAPER routes.
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    PLUGIN_MANUFACTURER_CODE Autl
    PLUGIN_CODE Mdt1
    FORMATS VST3 Standalone
    PRODUCT_NAME "miditin 1"
    # Not installed to ~/.vst3 -- the DAW is pointed at this build output, so no stale copy loads.
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Shared between the plugin and the headless tests. Anything that is not UI belongs here.
set(MDT_CORE_SOURCES
    Source/Model/ParamRegistry.cpp
    Source/Model/Patch.cpp
    Source/Engine/Engine.cpp)

# Drawing code the plugin and the snapshot tool share.
set(MDT_UI_SOURCES
    Source/UI/Fonts.cpp
    Source/UI/Controls.cpp
    Source/UI/Views.cpp
    Source/UI/Shell.cpp)

set(MDT_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(Miditin PRIVATE
    ${MDT_CORE_SOURCES}
    ${MDT_UI_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Miditin PUBLIC ${MDT_DEFINITIONS} JUCE_VST3_CAN_REPLACE_VST2=0)
target_include_directories(Miditin PRIVATE Source)
target_link_libraries(Miditin
    PRIVATE
        MiditinData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -- Headless test binary ------------------------------------------------------
juce_add_console_app(MiditinTests PRODUCT_NAME "MiditinTests")
target_sources(MiditinTests PRIVATE
    ${MDT_CORE_SOURCES}
    Tests/main.cpp
    Tests/ModelTests.cpp
    Tests/TransformerTests.cpp
    Tests/GenerateTests.cpp
    Tests/EnergyTests.cpp
    Tests/PulseTests.cpp
    Tests/EngineTests.cpp)
target_compile_definitions(MiditinTests PRIVATE ${MDT_DEFINITIONS})
target_include_directories(MiditinTests PRIVATE Source Tests)
target_link_libraries(MiditinTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# -- Offscreen UI snapshots ----------------------------------------------------
# Renders the editor to PNGs with no window and no host: the dev machine is a Wayland session, and
# an agent must never open a window on the user's display.
juce_add_console_app(MiditinSnap PRODUCT_NAME "MiditinSnap")
target_sources(MiditinSnap PRIVATE
    ${MDT_CORE_SOURCES}
    ${MDT_UI_SOURCES}
    Tools/RenderSnapshots.cpp)
target_compile_definitions(MiditinSnap PRIVATE ${MDT_DEFINITIONS})
target_include_directories(MiditinSnap PRIVATE Source)
target_link_libraries(MiditinSnap
    PRIVATE
        MiditinData
        juce::juce_gui_basics
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
