cmake_minimum_required(VERSION 3.22)
project(SwellRealisticGuitar VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_DIR to the JUCE source directory (tested with 8.0.4)")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
juce_add_plugin(SwellGuitar
    COMPANY_NAME "Swell Audio" BUNDLE_ID audio.swell.realisticguitar
    PLUGIN_MANUFACTURER_CODE Swel PLUGIN_CODE SgWv
    PRODUCT_NAME "Swell Realistic guitar"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE FORMATS VST3
    VST3_CATEGORIES Instrument Sampler COPY_PLUGIN_AFTER_BUILD FALSE)
juce_add_binary_data(GuitarArt SOURCES Assets/studio.jpg Assets/swell_mark.png
    Assets/unbounded_extrabold.ttf Assets/unbounded_medium.ttf Assets/geist_regular.ttf Assets/geist_semibold.ttf)
target_link_libraries(SwellGuitar PRIVATE GuitarArt)
target_sources(SwellGuitar PRIVATE Source/Bank.cpp Source/Processor.cpp Source/Editor.cpp)
target_compile_definitions(SwellGuitar PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SwellGuitar PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

option(SWELL_TESTS "Build the local audio and editor check" OFF)
if(SWELL_TESTS)
    add_executable(GuitarCheck Tests/Check.cpp)
    target_link_libraries(GuitarCheck PRIVATE SwellGuitar)
    target_include_directories(GuitarCheck PRIVATE Source $<TARGET_PROPERTY:SwellGuitar,INCLUDE_DIRECTORIES>)
    target_compile_definitions(GuitarCheck PRIVATE $<TARGET_PROPERTY:SwellGuitar,COMPILE_DEFINITIONS>)
endif()
