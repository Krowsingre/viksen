# Shared real-time engine. No platform, UI, file-format or product dependencies.
add_library(atmos_engine STATIC
  audio/AudioEngine.cpp
  audio/WorldStreamPlayer.cpp
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
