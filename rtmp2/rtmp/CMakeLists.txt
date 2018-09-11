#include(compiler/toolchain-arm-linux-rv1108.cmake)

cmake_minimum_required(VERSION 2.8)
project(rtmp)

add_compile_options(-W -w -std=c++11 -lpthread -fpermissive)

set(RTMP_SRC_FILES
	#src/rtmp_send264.cpp
	src/rtmp_stream.cpp
	#src/sps_decode.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_library(rtmp2 STATIC ${RTMP_SRC_FILES})
target_link_libraries(rtmp2 -lrtmp -lssl -lcrypto -lz)

