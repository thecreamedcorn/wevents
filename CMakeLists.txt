cmake_minimum_required(VERSION 3.8)
project(wevents)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS -pthread)

set(SOURCE_FILES "src/w_event(old).h" src/w_property.h examples.cpp src/w_event.h)
add_executable(wevents ${SOURCE_FILES})