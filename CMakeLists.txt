cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(CleanYourMind VERSION 0.1.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CYM_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
option(CYM_HEADLESS_TEST "Linux test build without optional system libraries" OFF)

if(CYM_JUCE_DIR)
    add_subdirectory(${CYM_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CYM_SOURCES
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Look.cpp
    Source/Gallery.cpp
    Source/Stats.cpp
    Source/Crop.cpp
    Source/Library.cpp)

if(APPLE)
    list(APPEND CYM_SOURCES Source/ClipboardImage.mm)
else()
    list(APPEND CYM_SOURCES Source/ClipboardImage.cpp)
endif()

juce_add_gui_app(CleanYourMind
    PRODUCT_NAME "Clean Your Mind"
    COMPANY_NAME "Primal Rage Lab"
    BUNDLE_ID "com.primalragelab.cleanyourmind"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    VERSION ${PROJECT_VERSION})

target_sources(CleanYourMind PRIVATE ${CYM_SOURCES})

# The interface text is Russian/English UTF-8 right in the source
if(MSVC)
    target_compile_options(CleanYourMind PRIVATE /utf-8)
endif()

target_compile_definitions(CleanYourMind PRIVATE
    CYM_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_APPLICATION_NAME_STRING="Clean Your Mind"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

if(CYM_HEADLESS_TEST)
    target_compile_definitions(CleanYourMind PRIVATE
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0)
endif()

target_link_libraries(CleanYourMind
    PRIVATE juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# Offline test tool: scans folders / projects and renders screenshots of the UI
if(CYM_HEADLESS_TEST)
    juce_add_console_app(CYMTest PRODUCT_NAME "CYMTest")
    target_sources(CYMTest PRIVATE
        Tests/Test.cpp
        Source/MainComponent.cpp
        Source/Look.cpp
        Source/Gallery.cpp
        Source/Stats.cpp
        Source/Crop.cpp
        Source/Library.cpp
        Source/ClipboardImage.cpp)
    target_include_directories(CYMTest PRIVATE Source)
    target_compile_definitions(CYMTest PRIVATE
        CYM_VERSION="test" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0)
    target_link_libraries(CYMTest PRIVATE juce::juce_gui_extra juce::juce_recommended_config_flags)
endif()
