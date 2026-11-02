# Shared real-time engine. No platform, UI, file-format or product dependencies.
add_library(atmos_engine STATIC
  audio/AudioEngine.cpp
  audio/WorldStreamPlayer.cpp
  audio/CaptureTap.cpp
  spatial/BinauralFactory.cpp
  spatial/SteamAudioRenderer.cpp
  dsp/Limiter.cpp
  dsp/Resampler.cpp
  environment/FeatureExtractor.cpp
  environment/MicTransform.cpp
  atmosphere/NoiseBed.cpp
  spatial/InterimStereoPanner.cpp
  rendering/OfflineRenderer.cpp
  dsp/FrequencyShifter.cpp
  environment/RoomMemory.cpp
  reactive/EnvironmentStates.cpp
)
target_include_directories(atmos_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(atmos_engine PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(atmos_engine PUBLIC Threads::Threads)
atmos_set_warnings(atmos_engine)

# Steam Audio (Apache-2.0) HRTF binaural rendering. Fetched by tools/fetch_steam_audio.sh;
# used automatically when present for this platform, otherwise the interim panner is used.
set(ATMOS_STEAM_AUDIO_DIR "${PROJECT_SOURCE_DIR}/third_party/steamaudio")
option(ATMOS_WITH_STEAM_AUDIO "Use Steam Audio for HRTF binaural rendering when available" ON)
if(ATMOS_WITH_STEAM_AUDIO AND EXISTS "${ATMOS_STEAM_AUDIO_DIR}/include/phonon.h")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${ATMOS_STEAM_AUDIO_DIR}/lib/linux-x64/libphonon.so")
    set(_phonon "${ATMOS_STEAM_AUDIO_DIR}/lib/linux-x64/libphonon.so")
  elseif(APPLE AND NOT IOS AND EXISTS "${ATMOS_STEAM_AUDIO_DIR}/lib/osx/libphonon.dylib")
    set(_phonon "${ATMOS_STEAM_AUDIO_DIR}/lib/osx/libphonon.dylib")
  elseif(WIN32 AND EXISTS "${ATMOS_STEAM_AUDIO_DIR}/lib/windows-x64/phonon.lib")
    set(_phonon "${ATMOS_STEAM_AUDIO_DIR}/lib/windows-x64/phonon.lib")
  endif()
  if(_phonon)
    message(STATUS "Steam Audio: ${_phonon}")
    target_compile_definitions(atmos_engine PUBLIC ATMOS_WITH_STEAM_AUDIO=1)
    target_include_directories(atmos_engine SYSTEM PRIVATE "${ATMOS_STEAM_AUDIO_DIR}/include")
    target_link_libraries(atmos_engine PUBLIC "${_phonon}")
    if(NOT WIN32)
      get_filename_component(_phonon_dir "${_phonon}" DIRECTORY)
      set_target_properties(atmos_engine PROPERTIES INTERFACE_LINK_OPTIONS "-Wl,-rpath,${_phonon_dir}")
    endif()
  else()
    message(STATUS "Steam Audio: no library for this platform; using the interim panner")
  endif()
endif()
