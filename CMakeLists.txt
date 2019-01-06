cmake_minimum_required(VERSION 3.13)
project(draft)

set(CMAKE_CXX_STANDARD 14)

add_executable(draft SimulationMain.h SimulationMain.cpp distributions.h distributions.cpp)
