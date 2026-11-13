cmake_minimum_required(VERSION 3.22)
project(SwellRealisticGuitar VERSION 1.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_DIR to the JUCE source directory (tested with 8.0.4)")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")

# Monocypher (BSD-2 / CC0): Ed25519 check of the Swell licence files.
include(FetchContent)
FetchContent_Declare(monocypher
    GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git
    GIT_TAG 4.0.3
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR none)
FetchContent_MakeAvailable(monocypher)
add_library(monocypher STATIC "${monocypher_SOURCE_DIR}/src/monocypher.c" "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c")
target_include_directories(monocypher PUBLIC "${monocypher_SOURCE_DIR}/src" "${monocypher_SOURCE_DIR}/src/optional")
set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(guitar_formats VST3 Standalone)
if(APPLE)
    list(APPEND guitar_formats AU)
endif()

juce_add_plugin(SwellGuitar
    COMPANY_NAME "Swell Audio" BUNDLE_ID audio.swell.realisticguitar
    PLUGIN_MANUFACTURER_CODE Swel PLUGIN_CODE SgWv
    PRODUCT_NAME "Swell Realistic guitar"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE FORMATS ${guitar_formats}
    ICON_BIG Assets/guitar-icon.png
    VST3_CATEGORIES Instrument Sampler COPY_PLUGIN_AFTER_BUILD FALSE)
juce_add_binary_data(GuitarArt SOURCES Assets/studio.jpg Assets/swell_mark.png
    Assets/unbounded_extrabold.ttf Assets/unbounded_medium.ttf Assets/geist_regular.ttf Assets/geist_semibold.ttf)
target_link_libraries(SwellGuitar PRIVATE GuitarArt)
target_sources(SwellGuitar PRIVATE Source/Bank.cpp Source/Processor.cpp Source/Editor.cpp Source/Licence/SwellLicence.cpp)
target_compile_definitions(SwellGuitar PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SwellGuitar PRIVATE monocypher juce::juce_cryptography juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

option(SWELL_TESTS "Build the local audio and editor check" OFF)
if(SWELL_TESTS)
    add_executable(GuitarCheck Tests/Check.cpp)
    target_link_libraries(GuitarCheck PRIVATE SwellGuitar)
    target_include_directories(GuitarCheck PRIVATE Source $<TARGET_PROPERTY:SwellGuitar,INCLUDE_DIRECTORIES>)
    target_compile_definitions(GuitarCheck PRIVATE $<TARGET_PROPERTY:SwellGuitar,COMPILE_DEFINITIONS>)
    add_executable(GuitarCapture Tests/Capture.cpp)
    target_link_libraries(GuitarCapture PRIVATE SwellGuitar "-framework CoreFoundation")
    target_include_directories(GuitarCapture PRIVATE Source $<TARGET_PROPERTY:SwellGuitar,INCLUDE_DIRECTORIES>)
    target_compile_definitions(GuitarCapture PRIVATE $<TARGET_PROPERTY:SwellGuitar,COMPILE_DEFINITIONS>)
endif()
