cmake_minimum_required(VERSION 3.8)
project(wevents)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES library.cpp library.h)
add_library(wevents ${SOURCE_FILES})