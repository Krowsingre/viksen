add_executable(atmos_tests
  TestMain.cpp
  unit/DspTests.cpp
  unit/EngineTests.cpp
  unit/ProjectTests.cpp
  unit/LiveSessionTests.cpp
  unit/ReactiveTests.cpp
  unit/WorldTests.cpp
  unit/SectionTests.cpp
  unit/LunarTests.cpp
  unit/MoonEventTests.cpp
  unit/ReactorTests.cpp
  unit/RoomInstrumentTests.cpp
  unit/SpatialTests.cpp
  unit/RecordingTests.cpp
  unit/SessionDocumentTests.cpp
  integration/RoundTripTests.cpp
)
target_link_libraries(atmos_tests PRIVATE atmos_core)
target_compile_definitions(atmos_tests PRIVATE ATMOS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
atmos_set_warnings(atmos_tests)

foreach(suite dsp spatial engine environment reactive automation worlds recording wav clip project session integration)
  add_test(NAME ${suite} COMMAND atmos_tests ${suite})
endforeach()

add_executable(atmos_benchmark performance/EngineBenchmark.cpp)
target_link_libraries(atmos_benchmark PRIVATE atmos_engine)

if(TARGET atmos_desktop_host)
  target_sources(atmos_tests PRIVATE unit/DesktopHostTests.cpp)
  target_link_libraries(atmos_tests PRIVATE atmos_desktop_host)
  add_test(NAME desktop COMMAND atmos_tests desktop)
endif()

if(TARGET viksen_engine)
  # The C interface is tested through the shared library, as the Windows app uses it.
  add_executable(viksen_capi_tests TestMain.cpp unit/CApiTests.cpp)
  target_link_libraries(viksen_capi_tests PRIVATE viksen_engine atmos_core)
  atmos_set_warnings(viksen_capi_tests)
  add_test(NAME capi COMMAND viksen_capi_tests desktop)
  if(WIN32)
    # Run against the DLL (and Steam Audio's) next to the test executable.
    add_custom_command(TARGET viksen_capi_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:viksen_capi_tests> $<TARGET_FILE_DIR:viksen_capi_tests>
      COMMAND_EXPAND_LISTS)
  endif()
endif()
