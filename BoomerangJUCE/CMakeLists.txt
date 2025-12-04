cmake_minimum_required(VERSION 3.22)

project(Boomerang VERSION 2.0.0.1)

# Add JUCE as a subdirectory (you'll need to clone JUCE)
add_subdirectory(JUCE)

# Plugin configuration
juce_add_plugin(Boomerang
    COMPANY_NAME "MC Music Workshop"
    BUNDLE_ID com.MCMusicWorkshop.Boomerang
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Boomerang+ needs microphone access to record loops."
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE MCMW
    PLUGIN_CODE BOOM
    FORMATS Standalone
    PRODUCT_NAME "Boomerang+")

# Source files
target_sources(Boomerang
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LooperEngine.cpp)

# Compile definitions
target_compile_definitions(Boomerang
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Required JUCE modules
target_link_libraries(Boomerang
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Suppress specific warnings for now
target_compile_options(Boomerang
    PRIVATE
        -Wno-unused-parameter
        -Wno-shadow
        -Wno-sign-conversion
        -Wno-switch
        -Wno-unused-variable)
