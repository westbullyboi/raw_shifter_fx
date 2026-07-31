cmake_minimum_required(VERSION 3.22)

project(ShifterFX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SHIFTERFX_BUILD_PLUGIN "Build the ShifterFX JUCE VST3/AU/Standalone plugin (requires JUCE)" ON)
option(SHIFTERFX_BUILD_TESTS "Build the framework-independent DSP unit tests" ON)

# --- DSP core -----------------------------------------------------------
# A tiny interface target listing the JUCE-independent DSP/Utility sources.
# Both the plugin target and the test target compile these same files, so
# there is only one implementation of the algorithms to keep correct.
set(SHIFTERFX_DSP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/ShifterEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/ShifterEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/RingBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/TransportInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/StepClock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/StepRemap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/GateEnvelope.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/PanLaw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/StepFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/Bitcrusher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/EchoEffect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Utility/Constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Utility/Math.h
)

if (SHIFTERFX_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    # AU (Audio Unit) only builds on Apple platforms - Logic Pro and GarageBand only load AU,
    # not VST3, so this format is required for those hosts even though VST3 alone covers most
    # other DAWs (Ableton, Studio One, Reaper, Bitwig, ...) on every OS.
    set(SHIFTERFX_PLUGIN_FORMATS VST3 Standalone)
    if (APPLE)
        list(APPEND SHIFTERFX_PLUGIN_FORMATS AU)
    endif()

    juce_add_plugin(ShifterFX
        COMPANY_NAME "ShifterFX"
        PLUGIN_MANUFACTURER_CODE Shfx
        PLUGIN_CODE Sfx2
        FORMATS ${SHIFTERFX_PLUGIN_FORMATS}
        PRODUCT_NAME "RAW SHIFTER FX"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect
    )

    target_sources(ShifterFX PRIVATE
        ${SHIFTERFX_DSP_CORE_SOURCES}
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/UI/Theme.h
        Source/UI/AmountKnob.h
        Source/UI/StepGridView.h
    )

    target_compile_definitions(ShifterFX PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(ShifterFX PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endif()

if (SHIFTERFX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
