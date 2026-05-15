cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(P2PScreenSharingTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(Stb REQUIRED)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Vcpkg manages FFmpeg packages via pkgconfig or direct paths. 
# Safe manual find for Windows/vcpkg:
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIB avcodec)
find_library(AVUTIL_LIB avutil)
find_library(SWSCALE_LIB swscale)

find_path(SRT_INCLUDE_DIR NAMES srt/srt.h)
find_library(SRT_LIBRARY NAMES srt libsrt)

if(SRT_INCLUDE_DIR AND SRT_LIBRARY)
    message(STATUS "Found SRT: ${SRT_LIBRARY}")
    add_library(SRT::srt UNKNOWN IMPORTED)
    set_target_properties(SRT::srt PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SRT_INCLUDE_DIR}"
        IMPORTED_LOCATION "${SRT_LIBRARY}"
    )
else()
    message(FATAL_ERROR "Could not find SRT library. Checked: ${SRT_INCLUDE_DIR} and ${SRT_LIBRARY}")
endif()

set(APP_SOURCES
    "src/main.cpp"
    "src/application.cpp"
    "src/capturer/video-capturer.cpp"
    "src/encoder/stream-encoder.cpp"
    "src/decoder/stream-decoder.cpp"
    "src/ui/application-ui.cpp"
    "src/network/srt-sender.cpp"
)

add_executable(CaptureApp ${APP_SOURCES})

target_include_directories(CaptureApp PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/src/inc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/config"
  ${Stb_INCLUDE_DIR}
)

target_link_libraries(CaptureApp PRIVATE 
  ${OpenCV_LIBS}
  ${AVCODEC_LIB} 
  ${AVUTIL_LIB} 
  ${SWSCALE_LIB}
  glfw
  imgui::imgui
  OpenGL::GL
  SRT::srt
  ws2_32
)