# Projects, assets, serialization. Depends on the engine's data types only.
add_library(atmos_core STATIC
  assets/WavFile.cpp
  assets/ClipPreparation.cpp
  projects/ProjectManifest.cpp
  projects/ProjectPackage.cpp
  projects/ProjectPlayback.cpp
  serialization/Ids.cpp
  session/LiveSession.cpp
  worlds/WorldLifecycle.cpp
  worlds/WorldConditions.cpp
)
target_include_directories(atmos_core PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(atmos_core SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party)
target_link_libraries(atmos_core PUBLIC atmos_engine)
atmos_set_warnings(atmos_core)
