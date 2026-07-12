cmake_minimum_required(VERSION 3.10)

project(SSD_Simulator)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(ssd_simulator
    src/main.cpp
    src/Page.cpp
    src/Block.cpp
    src/FlashMemory.cpp
    src/FTLMapper.cpp
    src/SSDController.cpp
    src/WriteRequest.cpp
    src/RequestQueue.cpp
    # Phase 1: GC + Stats
    src/StatsTracker.cpp
    src/GarbageCollector.cpp
    # Phase 2: Latency Model
    src/LatencyModel.cpp
)
