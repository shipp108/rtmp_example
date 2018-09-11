
cmake_minimum_required(VERSION 2.8)
project(rtmp)

add_compile_options(-W -w -std=c++11 -lpthread -fpermissive)


set(RTMP_SRC2_FILES
	src/librtmp_send264.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_executable(testRTMP src/simplest_librtmp_send264.cpp  ${RTMP_SRC2_FILES})

target_link_libraries(testRTMP -lrtmp -lssl -lcrypto -lz)
