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
    ../../third_party/melonDS/src
    ../../third_party/libretro-common/include
)

add_test(NAME melondsds_packet_test COMMAND melondsds_packet_test)

add_executable(melondsds_mp_state_test
    mp_state_test.cpp
    ../../src/libretro/net/mp.cpp
    ../../src/libretro/net/packet.cpp
)
target_compile_features(melondsds_mp_state_test PRIVATE cxx_std_17)
target_compile_definitions(melondsds_mp_state_test PRIVATE LUBAN_NDS_WIRELESS_NATIVE_TEST)
target_include_directories(melondsds_mp_state_test PRIVATE
    ../../src/libretro
    ../../src/libretro/net
    ../../third_party/melonDS/src
    ../../third_party/date/include
    ../../third_party/fmt/include
    ../../third_party/glm
    ../../third_party/libretro-common/include
)

add_test(NAME melondsds_mp_state_test COMMAND melondsds_mp_state_test)

add_executable(melondsds_nds_wireless_runtime_test
    nds_wireless_runtime_test.cpp
    ../../../../frontend/nds_wireless_runtime.cpp
)
target_compile_features(melondsds_nds_wireless_runtime_test PRIVATE cxx_std_17)
target_compile_definitions(melondsds_nds_wireless_runtime_test PRIVATE LUBAN_NDS_WIRELESS_NATIVE_TEST)
target_include_directories(melondsds_nds_wireless_runtime_test PRIVATE
    ../../../../frontend
)

add_test(NAME melondsds_nds_wireless_runtime_test COMMAND melondsds_nds_wireless_runtime_test)
