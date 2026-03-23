cmake_minimum_required(VERSION 3.22)
project(AdaptiveIR VERSION 1.0.0)

# =========================
# Configurazioni modificabili (CMake Cache)
# =========================
set(JUCE_DIR "" CACHE PATH "Path to local JUCE clone (optional). If empty, JUCE will be fetched via FetchContent.")
set(ADAPTIVEIR_COMPANY_NAME "Tuonome" CACHE STRING "Plugin company name")
set(ADAPTIVEIR_MANUFACTURER_CODE "TuoM" CACHE STRING "4-char manufacturer code")
set(ADAPTIVEIR_PLUGIN_CODE "AdIR" CACHE STRING "4-char plugin code")
set(ADAPTIVEIR_PRODUCT_NAME "AdaptiveIR" CACHE STRING "Product name")

# =========================
# JUCE: local path OR FetchContent
# =========================
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(STATUS "Using local JUCE from: ${JUCE_DIR}")
  add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
  )
  FetchContent_MakeAvailable(JUCE)
endif()

# =========================
# Plugin target
# =========================
juce_add_plugin(AdaptiveIR
  COMPANY_NAME "${ADAPTIVEIR_COMPANY_NAME}"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE ${ADAPTIVEIR_MANUFACTURER_CODE}
  PLUGIN_CODE ${ADAPTIVEIR_PLUGIN_CODE}
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "${ADAPTIVEIR_PRODUCT_NAME}"
)

# =========================
# Sources
# =========================
target_sources(AdaptiveIR
  PRIVATE
    src/Config.h
    src/Parameters.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/components/EQCurveDisplay.h
    src/dsp/TransientDetector.h
    src/dsp/SpectralAnalyzer.h
    src/dsp/DynamicEQ.h
    src/dsp/Utils.h
)

# Common definitions
target_compile_definitions(AdaptiveIR
  PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

# Linux deps (GTK/WebKit/Curl) - as in your current setup
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-x11-3.0 webkit2gtk-4.1 libcurl)
  target_include_directories(AdaptiveIR PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_libraries(AdaptiveIR PRIVATE ${GTK3_LIBRARIES})
endif()

target_link_libraries(AdaptiveIR
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)