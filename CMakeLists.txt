# CMake build for dolbye2sadm converter
#
# Run "cmake" to generate the build files for your platform

cmake_minimum_required(VERSION 3.12.0)

set(CMAKE_CXX_STANDARD 17)

# Use folders (for IDE project grouping)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Source additional modules from the "cmake" directory
list(APPEND CMAKE_MODULE_PATH
     "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Project version
project(dolbye2sadm
        VERSION 1.0
        LANGUAGES C CXX)

# Search the project binary dir for config.h
include_directories(
  ${PROJECT_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}
)

#set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
#set(Boost_USE_DEBUG_LIBS         ON)  # ignore debug libs and
#set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
#set(Boost_USE_MULTITHREADED      ON)
#set(Boost_USE_STATIC_RUNTIME    OFF)
find_package("XercesC" REQUIRED)
find_package("Boost" REQUIRED)


add_library(dolbye2sadm_lib src/ddeinfo.h src/dolbye.cpp src/dolbye_file.cpp src/dolbye_file.h src/dolbye_parser.cpp src/dolbye_parser.h )

target_link_libraries(dolbye2sadm_lib XercesC::XercesC Boost::headers)

add_executable(dolbye2sadm src/dolbye2sadm_main.cpp)

target_link_libraries(dolbye2sadm dolbye2sadm_lib)

install(TARGETS dolbye2sadm DESTINATION bin)


