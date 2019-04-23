cmake_minimum_required(VERSION 3.7)
project(GrafHf)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-write-strings")
SET(GCC_COVERAGE_LINK_FLAGS    "")
set(SOURCE_FILES Skeleton.cpp framework.cpp)

include_directories(include)
link_directories(lib)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} opengl32 freeglut glew32)