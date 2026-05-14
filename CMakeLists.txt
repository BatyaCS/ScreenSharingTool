cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(P2PScreenSharingTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(Stb REQUIRED)

set(APP_SOURCES 
    "src/main.cpp"
    "src/capturer/video-capturer.cpp"
)

add_executable(CaptureApp ${APP_SOURCES})

target_include_directories(CaptureApp PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/src/inc"
  ${Stb_INCLUDE_DIR}
)

target_link_libraries(CaptureApp PRIVATE 
    ${OpenCV_LIBS}
)