cmake_minimum_required(VERSION 3.16)
project(tts-front-cpp VERSION 0.1.0 LANGUAGES CXX)

option(TTS_FRONT_BUILD_TESTS "Build tests" ON)
option(TTS_FRONT_BUILD_EXAMPLES "Build examples" ON)
option(TTS_FRONT_BUILD_BENCHMARKS "Build benchmarks" ON)
option(TTS_FRONT_ENABLE_ONNX_STRESS "Build the private Silero ONNX backend" OFF)
set(TTS_FRONT_ONNX_RUNTIME_ROOT "" CACHE PATH "ONNX Runtime C/C++ package root")
set(TTS_FRONT_CLANG_FORMAT "" CACHE STRING "clang-format executable for format targets")

file(GLOB_RECURSE TTS_FRONT_FORMAT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp")
if(NOT TTS_FRONT_CLANG_FORMAT)
  find_program(TTS_FRONT_CLANG_FORMAT clang-format)
endif()
if(TTS_FRONT_CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${TTS_FRONT_CLANG_FORMAT} -i ${TTS_FRONT_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting C++ sources with clang-format")
  add_custom_target(format-check
    COMMAND ${TTS_FRONT_CLANG_FORMAT} --dry-run --Werror ${TTS_FRONT_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking C++ formatting with clang-format")
else()
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found; install it or set TTS_FRONT_CLANG_FORMAT"
    COMMAND ${CMAKE_COMMAND} -E false)
  add_custom_target(format-check
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found; install it or set TTS_FRONT_CLANG_FORMAT"
    COMMAND ${CMAKE_COMMAND} -E false)
endif()

add_library(tts_front
    src/text_frontend.cpp
    src/pronunciation_dictionary.cpp
    src/detail/utf8.cpp
    src/detail/silero_asset_hash.cpp
    src/detail/silero_bundle.cpp
    src/detail/silero_sentence.cpp
    src/detail/silero_stress_backend.cpp
)
add_library(TtsFront::tts_front ALIAS tts_front)
target_compile_features(tts_front PUBLIC cxx_std_17)
target_include_directories(tts_front PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
if(MSVC)
  target_compile_options(tts_front PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(tts_front PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(TTS_FRONT_ENABLE_ONNX_STRESS)
  if(NOT TTS_FRONT_ONNX_RUNTIME_ROOT)
    message(FATAL_ERROR
            "TTS_FRONT_ENABLE_ONNX_STRESS requires TTS_FRONT_ONNX_RUNTIME_ROOT")
  endif()
  find_path(TTS_FRONT_ONNX_RUNTIME_INCLUDE_DIR
            NAMES onnxruntime_cxx_api.h
            PATHS "${TTS_FRONT_ONNX_RUNTIME_ROOT}/include"
            NO_DEFAULT_PATH)
  find_library(TTS_FRONT_ONNX_RUNTIME_LIBRARY
               NAMES onnxruntime
               PATHS "${TTS_FRONT_ONNX_RUNTIME_ROOT}/lib"
               NO_DEFAULT_PATH)
  if(NOT TTS_FRONT_ONNX_RUNTIME_INCLUDE_DIR OR NOT TTS_FRONT_ONNX_RUNTIME_LIBRARY)
    message(FATAL_ERROR
            "TTS_FRONT_ONNX_RUNTIME_ROOT must contain include/onnxruntime_cxx_api.h and lib/onnxruntime")
  endif()
  target_sources(tts_front PRIVATE src/detail/silero_ort_session_owner.cpp)
  target_compile_definitions(tts_front PRIVATE TTS_FRONT_ENABLE_ONNX_STRESS=1)
  target_include_directories(tts_front PRIVATE "${TTS_FRONT_ONNX_RUNTIME_INCLUDE_DIR}")
  target_link_libraries(tts_front PRIVATE "${TTS_FRONT_ONNX_RUNTIME_LIBRARY}")
endif()

function(tts_front_configure_target target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
  endif()
endfunction()

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
  tts_front_configure_target(tts_front_tests)
  target_link_libraries(tts_front_tests PRIVATE TtsFront::tts_front)
  target_compile_definitions(tts_front_tests PRIVATE TTS_FRONT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME tts_front_tests COMMAND tts_front_tests)
  add_executable(tts_front_fixture_tests tests/fixture_tests.cpp)
  tts_front_configure_target(tts_front_fixture_tests)
  target_link_libraries(tts_front_fixture_tests PRIVATE TtsFront::tts_front)
  target_compile_definitions(tts_front_fixture_tests PRIVATE TTS_FRONT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME tts_front_fixture_tests COMMAND tts_front_fixture_tests)
  add_executable(tts_front_header_self_test tests/header_self_contained.cpp)
  tts_front_configure_target(tts_front_header_self_test)
  target_link_libraries(tts_front_header_self_test PRIVATE TtsFront::tts_front)
  target_compile_definitions(tts_front_header_self_test PRIVATE TTS_FRONT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME tts_front_header_self_test COMMAND tts_front_header_self_test)
  add_executable(tts_front_silero_asset_hash_tests tests/silero_asset_hash_tests.cpp)
  tts_front_configure_target(tts_front_silero_asset_hash_tests)
  target_include_directories(tts_front_silero_asset_hash_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(tts_front_silero_asset_hash_tests PRIVATE TtsFront::tts_front)
  add_test(NAME tts_front_silero_asset_hash_tests COMMAND tts_front_silero_asset_hash_tests)
  add_executable(tts_front_silero_bundle_tests tests/silero_bundle_tests.cpp)
  tts_front_configure_target(tts_front_silero_bundle_tests)
  target_include_directories(tts_front_silero_bundle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(tts_front_silero_bundle_tests PRIVATE TtsFront::tts_front)
  if(TTS_FRONT_ENABLE_ONNX_STRESS)
    target_compile_definitions(tts_front_silero_bundle_tests PRIVATE TTS_FRONT_ENABLE_ONNX_STRESS=1)
  endif()
  add_test(NAME tts_front_silero_bundle_tests COMMAND tts_front_silero_bundle_tests)
endif()
if(TTS_FRONT_BUILD_EXAMPLES)
  foreach(example normalize_ru normalize_en pronunciation_dictionary stress_demo)
    add_executable(${example} examples/${example}.cpp)
    tts_front_configure_target(${example})
    target_link_libraries(${example} PRIVATE TtsFront::tts_front)
  endforeach()
endif()
if(TTS_FRONT_BUILD_BENCHMARKS)
  add_executable(tts_front_benchmark benchmarks/normalize_benchmark.cpp)
  tts_front_configure_target(tts_front_benchmark)
  target_link_libraries(tts_front_benchmark PRIVATE TtsFront::tts_front)
endif()
