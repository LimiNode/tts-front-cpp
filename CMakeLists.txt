cmake_minimum_required(VERSION 3.16)
project(tts-front-cpp VERSION 0.1.0 LANGUAGES CXX)

option(TTS_FRONT_BUILD_TESTS "Build tests" ON)
option(TTS_FRONT_BUILD_EXAMPLES "Build examples" ON)
option(TTS_FRONT_BUILD_BENCHMARKS "Build benchmarks" ON)

add_library(tts_front
    src/text_frontend.cpp
    src/pronunciation_dictionary.cpp
)
add_library(TtsFront::tts_front ALIAS tts_front)
target_compile_features(tts_front PUBLIC cxx_std_17)
target_include_directories(tts_front PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
if(MSVC)
  target_compile_options(tts_front PRIVATE /W4 /permissive-)
else()
  target_compile_options(tts_front PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS tts_front EXPORT TtsFrontTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT TtsFrontTargets NAMESPACE TtsFront:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TtsFront)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/TtsFrontConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
configure_package_config_file(cmake/TtsFrontConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TtsFrontConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TtsFront)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TtsFrontConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/TtsFrontConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TtsFront)

if(TTS_FRONT_BUILD_TESTS)
  enable_testing()
  add_executable(tts_front_tests tests/text_frontend_tests.cpp)
  target_link_libraries(tts_front_tests PRIVATE TtsFront::tts_front)
  add_test(NAME tts_front_tests COMMAND tts_front_tests)
endif()
if(TTS_FRONT_BUILD_EXAMPLES)
  foreach(example normalize_ru normalize_en pronunciation_dictionary stress_demo)
    add_executable(${example} examples/${example}.cpp)
    target_link_libraries(${example} PRIVATE TtsFront::tts_front)
  endforeach()
endif()
if(TTS_FRONT_BUILD_BENCHMARKS)
  add_executable(tts_front_benchmark benchmarks/normalize_benchmark.cpp)
  target_link_libraries(tts_front_benchmark PRIVATE TtsFront::tts_front)
endif()
