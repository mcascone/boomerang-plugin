cmake_minimum_required(VERSION 3.22)

project(Boomerang VERSION 2.0.0.4)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build universal binary for both Intel and Apple Silicon
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")
endif()

# Get git commit hash (captured at CMake configure time)
# To update after a commit: cd build && cmake . && make -j8
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# If git command fails, set a default value
if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()

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
    PLUGIN_MANUFACTURER_CODE MCMW
    PLUGIN_CODE BOOM
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Boomerang+"
    # Skip automatic install to avoid signing warnings (issue #36)
    COPY_PLUGIN_AFTER_BUILD TRUE)

# ==============================================================================
# Code Signing Configuration (issue #36)
# ==============================================================================

# Optional: Enable official Apple Developer code signing for distribution
option(ENABLE_CODESIGNING "Enable official code signing for distribution" OFF)

# if(APPLE AND ENABLE_CODESIGNING)
#     # TODO: Replace with your actual certificate name and Team ID
#     # Find certificate: security find-identity -v -p codesigning
#     # Find Team ID: developer.apple.com > Account > Membership
#     set(CODESIGN_IDENTITY "Developer ID Application: Your Name (TEAMID)" CACHE STRING "Code signing identity")
#     set(DEVELOPMENT_TEAM "TEAMID" CACHE STRING "Apple Developer Team ID")
    
#     message(STATUS "Code signing enabled with identity: ${CODESIGN_IDENTITY}")
    
#     # Apply to plugin targets
#     set_target_properties(Boomerang_AU Boomerang_VST3 PROPERTIES
#         XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${CODESIGN_IDENTITY}"
#         XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${DEVELOPMENT_TEAM}"
#         XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED YES
#     )
# else()
#     message(STATUS "Using ad-hoc code signing for local development")
#     message(STATUS "To enable distribution signing: cmake -DENABLE_CODESIGNING=ON")
# endif()

# Post-build: Ad-hoc sign and install bundles cleanly (macOS only, issue #36)
# if(APPLE)
#     if(ENABLE_CODESIGNING)
#         # Use official signing identity
#         set(SIGN_COMMAND codesign --force --deep --sign "${CODESIGN_IDENTITY}" --timestamp)
#     else()
#         # Use ad-hoc signing for local development
#         set(SIGN_COMMAND codesign --force --deep --sign -)
#     endif()
    
#     # Sign AU
#     add_custom_command(TARGET Boomerang_AU POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:Boomerang_AU>/Contents/Resources"
#         COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_BUNDLE_DIR:Boomerang_AU>/Contents/Resources/.keep"
#         COMMAND ${SIGN_COMMAND} "$<TARGET_BUNDLE_DIR:Boomerang_AU>"
#         COMMAND ${CMAKE_COMMAND} -E copy_directory 
#                 "$<TARGET_BUNDLE_DIR:Boomerang_AU>"
#                 "$ENV{HOME}/Library/Audio/Plug-Ins/Components/Boomerang+.component"
#         COMMENT "Signing and installing AU plugin"
#         VERBATIM
#     )
    
#     # Sign VST3 - moduleinfo.json creates Resources folder, just sign it
#     add_custom_command(TARGET Boomerang_VST3 POST_BUILD
#         COMMAND ${SIGN_COMMAND} "$<TARGET_BUNDLE_DIR:Boomerang_VST3>"
#         COMMAND ${CMAKE_COMMAND} -E copy_directory 
#                 "$<TARGET_BUNDLE_DIR:Boomerang_VST3>"
#                 "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Boomerang+.vst3"
#         COMMENT "Signing and installing VST3 plugin"
#         VERBATIM
#     )
# endif()

# Source files
target_sources(Boomerang
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LooperEngine.cpp)

# Add binary resources (embeds image in binary)
juce_add_binary_data(BoomerangBinaryData
    SOURCES
        img/boomerang.jpg)

# Link binary data to plugin
target_link_libraries(Boomerang
    PRIVATE
        BoomerangBinaryData)

# Compile definitions
target_compile_definitions(Boomerang
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_SILENCE_DUPLICATE_AUDIO_PERMISSION_REQUESTS=1
        BOOMERANG_VERSION="${CMAKE_PROJECT_VERSION}"
        BOOMERANG_GIT_HASH="${GIT_HASH}")

# Required JUCE modules
target_link_libraries(Boomerang
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Warning suppressions removed - code is now warning-clean (issues #34-35 resolved)

# ==============================================================================
# Validation Targets (issue #38)
# ==============================================================================

# Option to enable ThreadSanitizer
option(ENABLE_TSAN "Enable ThreadSanitizer for race detection" OFF)

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=thread")
    
    # Find the TSan runtime library explicitly
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libclang_rt.tsan_osx_dynamic.dylib
        OUTPUT_VARIABLE TSAN_LIBRARY
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS "${TSAN_LIBRARY}")
        message(STATUS "Found TSan library: ${TSAN_LIBRARY}")
        link_libraries(${TSAN_LIBRARY})
    else()
        message(WARNING "TSan library not found, linking may fail")
    endif()
endif()

# Custom target for strict warnings build
add_custom_target(check-warnings
    COMMAND ${CMAKE_COMMAND} -E echo "Building with warnings as errors..."
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR}/warnings-build
            -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror" -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/warnings-build -j4
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building with -Werror to catch all warnings"
)

# Custom target to run pluginval (if installed)
find_program(PLUGINVAL pluginval PATHS 
    /Applications/pluginval.app/Contents/MacOS
    ~/Applications/pluginval.app/Contents/MacOS
    /usr/local/bin
)

if(PLUGINVAL)
    message(STATUS "Found pluginval: ${PLUGINVAL}")
    add_custom_target(validate
        COMMAND ${CMAKE_COMMAND} -E echo "Running pluginval on VST3..."
        COMMAND ${PLUGINVAL} --validate-in-process --verbose
                ${CMAKE_BINARY_DIR}/Boomerang_artefacts/VST3/Boomerang.vst3
        DEPENDS Boomerang
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Validating plugin with pluginval"
    )
else()
    message(STATUS "pluginval not found - 'make validate' target will not be available")
    message(STATUS "Download from: https://github.com/Tracktion/pluginval/releases")
endif()
