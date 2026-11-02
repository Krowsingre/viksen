add_executable(atmos_tests
  TestMain.cpp
  unit/DspTests.cpp
  unit/EngineTests.cpp
  unit/ProjectTests.cpp
  unit/LiveSessionTests.cpp
  unit/ReactiveTests.cpp
  unit/WorldTests.cpp
  unit/SpatialTests.cpp
  unit/RecordingTests.cpp
  unit/SessionDocumentTests.cpp
  integration/RoundTripTests.cpp
)
target_link_libraries(atmos_tests PRIVATE atmos_core)
atmos_set_warnings(atmos_tests)

foreach(suite dsp spatial engine environment reactive automation worlds recording wav clip project session integration)
  add_test(NAME ${suite} COMMAND atmos_tests ${suite})
endforeach()

add_executable(atmos_benchmark performance/EngineBenchmark.cpp)
target_link_libraries(atmos_benchmark PRIVATE atmos_engine)
