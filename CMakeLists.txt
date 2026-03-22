cmake_minimum_required(VERSION 3.22)
project(AdaptiveIR VERSION 1.0.0)

# Gestione portabile di JUCE
if(EXISTS "/home/test/JUCE")
    # Usa la versione locale per compilazioni veloci
    add_subdirectory(/home/test/JUCE ${CMAKE_BINARY_DIR}/JUCE)
else()
    # Scarica e configura JUCE automaticamente (per GitHub Actions o altri utenti)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Definisci il plugin
juce_add_plugin(AdaptiveIR
    COMPANY_NAME "Tuonome"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE # Copia automaticamente nella cartella plugin di sistema
    PLUGIN_MANUFACTURER_CODE TuoM
    PLUGIN_CODE AdIR
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "AdaptiveIR"
)

# Collega i file sorgente
target_sources(AdaptiveIR
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/dsp/TransientDetector.h
        src/dsp/SpectralAnalyzer.h
        src/dsp/DynamicEQ.h
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-x11-3.0 webkit2gtk-4.1 libcurl)

target_compile_definitions(AdaptiveIR
    PRIVATE
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

target_include_directories(AdaptiveIR
    PRIVATE
        ${GTK3_INCLUDE_DIRS}
)

target_link_libraries(AdaptiveIR
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp # Libreria DSP di JUCE (FFT, Convolution, IIR)
        ${GTK3_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)