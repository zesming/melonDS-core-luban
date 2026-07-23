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

add_executable(melondsds_bios_validation_test
    bios_validation_test.cpp
    ../../third_party/melonDS/src/CRC32.cpp
)
target_compile_features(melondsds_bios_validation_test PRIVATE cxx_std_17)
target_include_directories(melondsds_bios_validation_test PRIVATE
    ../../src/libretro/config
    ../../third_party/melonDS/src
    ../../third_party/span-lite/include
)

add_test(NAME melondsds_bios_validation_test COMMAND melondsds_bios_validation_test)

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

get_filename_component(LUBAN_NATIVE_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
get_filename_component(MELONDS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
get_filename_component(LUBAN_MELONDS_CORE_DIR "${LUBAN_NATIVE_CPP_DIR}/cores/melonds" REALPATH)
set(LUBAN_NDS_WIRELESS_RUNTIME_DIR "${LUBAN_NATIVE_CPP_DIR}/frontend")
set(LUBAN_NDS_WIRELESS_RUNTIME_SOURCE "${LUBAN_NDS_WIRELESS_RUNTIME_DIR}/nds_wireless_runtime.cpp")
set(LUBAN_NDS_WIRELESS_RUNTIME_HEADER "${LUBAN_NDS_WIRELESS_RUNTIME_DIR}/nds_wireless_runtime.h")

if (MELONDS_CORE_DIR STREQUAL LUBAN_MELONDS_CORE_DIR AND
    EXISTS "${LUBAN_NDS_WIRELESS_RUNTIME_SOURCE}" AND
    EXISTS "${LUBAN_NDS_WIRELESS_RUNTIME_HEADER}")
    add_executable(melondsds_nds_wireless_runtime_test
        nds_wireless_runtime_test.cpp
        "${LUBAN_NDS_WIRELESS_RUNTIME_SOURCE}"
    )
    target_compile_features(melondsds_nds_wireless_runtime_test PRIVATE cxx_std_17)
    target_compile_definitions(melondsds_nds_wireless_runtime_test PRIVATE LUBAN_NDS_WIRELESS_NATIVE_TEST)
    target_include_directories(melondsds_nds_wireless_runtime_test PRIVATE
        "${LUBAN_NDS_WIRELESS_RUNTIME_DIR}"
    )

    add_test(NAME melondsds_nds_wireless_runtime_test COMMAND melondsds_nds_wireless_runtime_test)
endif()
