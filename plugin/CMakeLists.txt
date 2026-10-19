# SPDX-License-Identifier: GPL-3.0-or-later
# P-800 - AU/VST3 wrapper around the ISLA Poly-800 core.
cmake_minimum_required(VERSION 3.22)
project(P800 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
set(JUCE_DIR "" CACHE PATH "Local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(P800_FORMATS VST3)
if(APPLE)
    list(APPEND P800_FORMATS AU)
elseif(UNIX)
    list(APPEND P800_FORMATS LV2)
endif()

juce_add_plugin(P800
    COMPANY_NAME "Ticco Ross"
    PRODUCT_NAME "P-800"
    BUNDLE_ID com.ticcoross.p800
    PLUGIN_MANUFACTURER_CODE Tcro
    PLUGIN_CODE Tp80
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    LV2URI https://github.com/Spunkmeyer77/Poly800
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${P800_FORMATS})

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_sources(P800 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PanelView.cpp
    ${CORE_DIR}/poly800_core_m6.c)

target_include_directories(P800 PRIVATE ${CORE_DIR} Source)

target_compile_definitions(P800 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(P800
    PRIVATE juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Engine/factory regression test (no JUCE needed): ctest --test-dir build
enable_testing()
add_executable(factory_render_test tests/factory_render_test.cpp ${CORE_DIR}/poly800_core_m6.c)
target_include_directories(factory_render_test PRIVATE ${CORE_DIR} Source)
if(NOT MSVC)
    target_link_libraries(factory_render_test PRIVATE m)
endif()
add_test(NAME factory_render COMMAND factory_render_test)

add_executable(joystick_test tests/joystick_test.cpp ${CORE_DIR}/poly800_core_m6.c)
target_include_directories(joystick_test PRIVATE ${CORE_DIR} Source)
if(NOT MSVC)
    target_link_libraries(joystick_test PRIVATE m)
endif()
add_test(NAME joystick COMMAND joystick_test)
