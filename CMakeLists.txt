cmake_minimum_required(VERSION 3.22)
project(Paul50Bag VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
add_subdirectory(JUCE)

juce_add_plugin(Paul50Bag
    COMPANY_NAME "SamSonight"
    COMPANY_WEBSITE "https://samsonight-lab.github.io"
    BUNDLE_ID "com.samsonight.paul50bag.plugin"
    PLUGIN_MANUFACTURER_CODE Smsn
    PLUGIN_CODE P50b
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "The Paul 50 Bag"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(Paul50BagUI SOURCES
    ui/index.html
    ui/fonts/cinzel-decorative-latin-700-normal.woff2
    ui/fonts/cinzel-decorative-latin-900-normal.woff2
    ui/fonts/cormorant-garamond-latin-500-italic.woff2
    ui/fonts/cormorant-garamond-latin-500-normal.woff2
    ui/fonts/cormorant-garamond-latin-600-italic.woff2
    ui/fonts/cormorant-garamond-latin-600-normal.woff2
    ui/fonts/federo-latin-400-normal.woff2
    ui/fonts/share-tech-mono-latin-400-normal.woff2)

target_sources(Paul50Bag PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(Paul50Bag PUBLIC
    JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
if(WIN32)
  target_compile_definitions(Paul50Bag PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()
target_link_libraries(Paul50Bag PRIVATE Paul50BagUI
    juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(P50_TESTS)
  juce_add_console_app(p50msgtest PRODUCT_NAME "p50msgtest")
  target_sources(p50msgtest PRIVATE test/msgtest.cpp Source/PluginProcessor.cpp)
  target_compile_definitions(p50msgtest PRIVATE P50_NO_EDITOR=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JucePlugin_Name="P50" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
  target_link_libraries(p50msgtest PRIVATE juce::juce_audio_processors juce::juce_audio_utils)
endif()
