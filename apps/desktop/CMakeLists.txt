# JUCE desktop application (Windows first, then macOS; Linux builds are for
# development/CI only). Enable with -DATMOS_BUILD_DESKTOP=ON.
#
# JUCE licensing: JUCE 8 is dual-licensed AGPLv3 / commercial. Shipping a
# closed-source build requires a commercial JUCE licence. See docs/DECISIONS.md.

include(FetchContent)

set(ATMOS_JUCE_VERSION "8.0.4" CACHE STRING "JUCE release tag")
set(ATMOS_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading")

if(ATMOS_JUCE_DIR)
  add_subdirectory(${ATMOS_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${ATMOS_JUCE_VERSION}
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE)
  FetchContent_MakeAvailable(juce)
endif()

set(DESKTOP_TARGET AtmosDesktop)

juce_add_gui_app(${DESKTOP_TARGET}
  PRODUCT_NAME "${PRODUCT_NAME}"
  COMPANY_NAME "${PRODUCT_NAME}"
  BUNDLE_ID "${PRODUCT_BUNDLE_ID}.desktop"
  VERSION ${PROJECT_VERSION}
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png"
  # macOS (ignored elsewhere): microphone usage string, hardened runtime with
  # audio-input entitlement, required for notarization.
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "The microphone lets you transform the sounds around you into a live, evolving experience. Audio is processed on this computer and is never recorded or uploaded unless you choose to record."
  HARDENED_RUNTIME_ENABLED TRUE
  HARDENED_RUNTIME_OPTIONS com.apple.security.device.audio-input)

target_sources(${DESKTOP_TARGET} PRIVATE
  Source/Main.cpp
  Source/MainComponent.cpp
  Source/AudioHost.cpp
  Source/ExperiencePage.cpp
  Source/SettingsPage.cpp
  ${PROJECT_SOURCE_DIR}/platform/desktop/DesktopAudioCommon.cpp
  ${PROJECT_SOURCE_DIR}/platform/desktop/DesktopMediaDecoder.cpp)

if(WIN32)
  target_sources(${DESKTOP_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/platform/windows/WindowsAudioDefaults.cpp)
elseif(APPLE)
  target_sources(${DESKTOP_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/platform/macos/MacAudioDefaults.cpp)
else()
  target_sources(${DESKTOP_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/platform/desktop/LinuxAudioDefaults.cpp)
endif()

target_compile_definitions(${DESKTOP_TARGET} PRIVATE
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${DESKTOP_TARGET},JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${DESKTOP_TARGET},JUCE_VERSION>"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_ASIO=0              # ASIO requires the Steinberg SDK licence; evaluate later
  JUCE_WASAPI=1
  JUCE_DIRECTSOUND=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(${DESKTOP_TARGET}
  PRIVATE
    atmos_core
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_gui_basics
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(WIN32)
  set_target_properties(${DESKTOP_TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
