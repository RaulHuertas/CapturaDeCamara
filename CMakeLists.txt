cmake_minimum_required(VERSION 2.8)
project( CapturaCamaraWeb )
set (CMAKE_CXX_STANDARD 14)
find_package( OpenCV )
find_package(Qt5 COMPONENTS Core REQUIRED)



include_directories( ${OpenCV_INCLUDE_DIRS} )
include_directories( include )
set(SOURCES VideoTest.cpp video_utils.cpp)
add_executable( CapturaCamaraWeb ${SOURCES} )

target_link_libraries( CapturaCamaraWeb ${OpenCV_LIBS} )
qt5_use_modules(CapturaCamaraWeb Core)
target_link_libraries( CapturaCamaraWeb pthread )
target_link_libraries( CapturaCamaraWeb realsense )
target_link_libraries( CapturaCamaraWeb realsense_projection )
target_link_libraries( CapturaCamaraWeb realsense_image )


