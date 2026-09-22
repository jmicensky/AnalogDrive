# Must be set before project() to take effect. Default build was arm64-only
# (Apple Silicon dev machine) which would refuse to launch on an Intel Mac;
# building both slices into one binary covers testers on either. Leaves the
# override open (-DCMAKE_OSX_ARCHITECTURES=...) for CI or anyone who wants a
# single-arch build for faster iteration.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
endif()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
endif()

cmake_minimum_required(VERSION 3.22)
project(ExpandedSaturation VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auto-installing into the system plugin folders makes sense on your own dev
# machine, but on CI it fails outright (no admin rights to write to e.g.
# /Library/Application Support/Avid) and isn't wanted anyway — CI just needs
# the raw build output to zip up. Pass -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF to
# disable it (see .github/workflows/build.yml).
option(JUCE_COPY_PLUGIN_AFTER_BUILD "Install the plugin into system plugin folders after building" ON)

# Point this at your JUCE source directory or use FetchContent
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "$ENV{HOME}/Downloads/OhmsteadMeter/JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE_build)

juce_add_plugin(ExpandedSaturation
    COMPANY_NAME "Saturation Sounds"
    BUNDLE_ID "com.saturationsounds.expandedsaturation"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${JUCE_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Jmce
    PLUGIN_CODE Esa1
    FORMATS VST3 AU AAX Standalone
    PRODUCT_NAME "Expanded Saturation"
)

juce_generate_juce_header(ExpandedSaturation)

# Factory presets — any XML dropped into Presets/Factory/ before a build gets
# embedded automatically as a read-only on-board preset (see PresetManager.cpp,
# which expects a BinaryData resource named "<Name>_preset_xml" — i.e. the file
# should be named "<Name>_preset.xml"). Empty as of 2026-09-21; this is the
# pipeline for once the ~10 auditioned presets are finalized. CONFIGURE_DEPENDS
# re-globs on the next build if the folder's contents change.
file(GLOB FACTORY_PRESET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Presets/Factory/*.xml")

# Curated set of bundled hardware profiles — kept in sync by hand with
# kBuiltInProfiles in Source/ModelManager.cpp.
juce_add_binary_data(ExpandedSaturation_BinaryData
    SOURCES
        ProfilerModels/SystemCalExperiment_Capi/SystemCalExperiment_Capi_model.json
        ProfilerModels/AmtekSS_Bias_Standard/AmtekSS_Bias_Standard_model.json
        "small texture.jpg"
        ${FACTORY_PRESET_FILES}
)

target_sources(ExpandedSaturation
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/AudioEngine.cpp
        Source/ModelManager.cpp
        Source/SaturationModeEngine.cpp
        Source/PluginParameters.cpp
        Source/SaturationProcessor.cpp
        Source/ArtifactFile.cpp
        Source/AnalysisEngine.cpp
        Source/ModelSelectorComponent.cpp
        Source/ModelSwitchComponent.cpp
        Source/TransferCurveDisplay.cpp
        Source/SpectrumDisplay.cpp
        Source/FilterShapeDisplay.cpp
        Source/LevelMeter.cpp
        Source/BypassOverlay.cpp
        Source/PresetManager.cpp
        Source/PresetBarComponent.cpp
        Source/KnobComponent.cpp
        Source/FilterComponent.cpp
        Source/ParameterAttachments.cpp
        Source/PluginState.cpp
        Source/RackLookAndFeel.cpp
)

target_compile_definitions(ExpandedSaturation
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ExpandedSaturation
    PRIVATE
        ExpandedSaturation_BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
