cmake_minimum_required(VERSION 3.16)
project(melondsds_native_tests LANGUAGES CXX)

include(CTest)

add_executable(melondsds_packet_test
    packet_test.cpp
    ../../src/libretro/net/packet.cpp
)
target_compile_features(melondsds_packet_test PRIVATE cxx_std_17)
target_include_directories(melondsds_packet_test PRIVATE
    ../../src/libretro/net
    ../../third_party/libretro-common/include
)

add_test(NAME melondsds_packet_test COMMAND melondsds_packet_test)
