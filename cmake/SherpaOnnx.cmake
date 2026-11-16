# SherpaOnnx.cmake
#
# sherpa-onnx runs NVIDIA Parakeet (ONNX export) for ParakeetAsrEngine. Same
# resolution order as WhisperCpp.cmake:
#   1. -DSHERPA_ONNX_SOURCE_DIR=/path/to/sherpa-onnx    (local checkout)
#   2. third_party/sherpa-onnx                          (vendored copy / submodule)
#   3. FetchContent from GitHub at SHERPA_ONNX_GIT_TAG  (requires network at configure time)
#
# sherpa-onnx downloads a prebuilt static onnxruntime for the target platform
# at configure time.

option(VOICETYPER_WITH_PARAKEET "Build with sherpa-onnx for Parakeet models" ON)

set(SHERPA_ONNX_SOURCE_DIR "" CACHE PATH
    "Path to a local sherpa-onnx checkout (overrides FetchContent)")
set(SHERPA_ONNX_GIT_TAG "v2.0.1" CACHE STRING
    "sherpa-onnx git tag to fetch when no local source is provided")

function(voicetyper_add_sherpa_onnx)
    if(NOT VOICETYPER_WITH_PARAKEET)
        message(STATUS "voiceTyper: building WITHOUT sherpa-onnx (no Parakeet)")
        return()
    endif()

    # Only the static C API library; nothing that pulls audio devices, servers
    # or TTS into the app.
    set(BUILD_SHARED_LIBS                      OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_C_API               ON  CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_BINARY              OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_BUILD_C_API_EXAMPLES       OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO           OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_WEBSOCKET           OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TTS                 OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PYTHON              OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TESTS               OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_JNI                 OFF CACHE BOOL "" FORCE)
    # Its deps use std::result_of, removed in C++20 (libc++ drops it).
    # Function scope keeps the app itself on C++20.
    set(CMAKE_CXX_STANDARD 17)

    set(_vendored "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")

    if(SHERPA_ONNX_SOURCE_DIR)
        message(STATUS "voiceTyper: using sherpa-onnx from SHERPA_ONNX_SOURCE_DIR=${SHERPA_ONNX_SOURCE_DIR}")
        add_subdirectory("${SHERPA_ONNX_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sherpa_onnx_build")
    elseif(EXISTS "${_vendored}/CMakeLists.txt")
        message(STATUS "voiceTyper: using vendored sherpa-onnx at ${_vendored}")
        add_subdirectory("${_vendored}" "${CMAKE_BINARY_DIR}/sherpa_onnx_build")
    else()
        message(STATUS "voiceTyper: fetching sherpa-onnx ${SHERPA_ONNX_GIT_TAG} via FetchContent")
        include(FetchContent)
        FetchContent_Declare(sherpa_onnx
            GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
            GIT_TAG        ${SHERPA_ONNX_GIT_TAG}
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(sherpa_onnx)
    endif()

    if(NOT TARGET sherpa-onnx-c-api)
        message(FATAL_ERROR
            "voiceTyper: expected a 'sherpa-onnx-c-api' CMake target from sherpa-onnx but none was found.")
    endif()
endfunction()
