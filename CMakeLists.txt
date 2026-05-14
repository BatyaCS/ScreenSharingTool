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

set(APP_SOURCES 
    "src/main.cpp"
    "src/capturer/video-capturer.cpp"
    "src/encoder/stream-encoder.cpp"
    "src/decoder/stream-decoder.cpp"
    "src/ui/application-ui.cpp"
)

add_executable(CaptureApp ${APP_SOURCES})

target_include_directories(CaptureApp PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/src/inc"
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
)