cmake_minimum_required(VERSION 3.20)
project(silero_native_preprocess LANGUAGES CXX)

enable_testing()

set(SILERO_NATIVE_ORT_ROOT "" CACHE PATH "ONNX Runtime C/C++ package root")

add_executable(silero_native_preprocess native_preprocess.cpp)
target_compile_features(silero_native_preprocess PRIVATE cxx_std_17)
add_executable(silero_native_wordpiece native_wordpiece.cpp)
target_compile_features(silero_native_wordpiece PRIVATE cxx_std_17)
add_executable(silero_native_utf8_smoke utf8_smoke.cpp)
target_compile_features(silero_native_utf8_smoke PRIVATE cxx_std_17)
add_test(NAME silero_native_utf8_smoke COMMAND silero_native_utf8_smoke)

if(SILERO_NATIVE_ORT_ROOT)
    find_path(SILERO_NATIVE_ORT_INCLUDE_DIR
              NAMES onnxruntime_cxx_api.h
              PATHS "${SILERO_NATIVE_ORT_ROOT}/include"
              NO_DEFAULT_PATH)
    find_library(SILERO_NATIVE_ORT_LIBRARY
                 NAMES onnxruntime
                 PATHS "${SILERO_NATIVE_ORT_ROOT}/lib"
                 NO_DEFAULT_PATH)
    if(NOT SILERO_NATIVE_ORT_INCLUDE_DIR OR NOT SILERO_NATIVE_ORT_LIBRARY)
        message(FATAL_ERROR "SILERO_NATIVE_ORT_ROOT must contain include/onnxruntime_cxx_api.h and lib/onnxruntime")
    endif()
    add_executable(silero_native_ort_probe native_ort_probe.cpp)
    target_compile_features(silero_native_ort_probe PRIVATE cxx_std_17)
    target_include_directories(silero_native_ort_probe PRIVATE "${SILERO_NATIVE_ORT_INCLUDE_DIR}")
    target_link_libraries(silero_native_ort_probe PRIVATE "${SILERO_NATIVE_ORT_LIBRARY}")
    add_executable(silero_native_sentence native_sentence.cpp)
    target_compile_features(silero_native_sentence PRIVATE cxx_std_17)
    target_include_directories(silero_native_sentence PRIVATE "${SILERO_NATIVE_ORT_INCLUDE_DIR}")
    target_link_libraries(silero_native_sentence PRIVATE "${SILERO_NATIVE_ORT_LIBRARY}")
    if(WIN32)
        add_custom_command(TARGET silero_native_ort_probe POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   "${SILERO_NATIVE_ORT_ROOT}/lib/onnxruntime.dll"
                                   "$<TARGET_FILE_DIR:silero_native_ort_probe>")
        add_custom_command(TARGET silero_native_sentence POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   "${SILERO_NATIVE_ORT_ROOT}/lib/onnxruntime.dll"
                                   "$<TARGET_FILE_DIR:silero_native_sentence>")
    else()
        set_target_properties(silero_native_ort_probe PROPERTIES
                              BUILD_RPATH "${SILERO_NATIVE_ORT_ROOT}/lib")
        set_target_properties(silero_native_sentence PROPERTIES
                              BUILD_RPATH "${SILERO_NATIVE_ORT_ROOT}/lib")
    endif()
    if(MSVC)
        target_compile_options(silero_native_ort_probe PRIVATE /W4 /utf-8)
        target_compile_options(silero_native_sentence PRIVATE /W4 /utf-8)
    else()
        target_compile_options(silero_native_ort_probe PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(silero_native_sentence PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
if(MSVC)
    target_compile_options(silero_native_preprocess PRIVATE /W4 /utf-8)
    target_compile_options(silero_native_wordpiece PRIVATE /W4 /utf-8)
    target_compile_options(silero_native_utf8_smoke PRIVATE /W4 /utf-8)
else()
    target_compile_options(silero_native_preprocess PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(silero_native_wordpiece PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(silero_native_utf8_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
