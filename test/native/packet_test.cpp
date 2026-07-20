#include "mp.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using MelonDsDs::HeaderSize;
using MelonDsDs::MaxPacketPayloadSize;
using MelonDsDs::Packet;
using MelonDsDs::ParsePacket;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void RequirePayload(const Packet &packet, const std::vector<uint8_t> &expected) {
    Require(packet.Length() == expected.size(), "payload length mismatch");
    const auto *actual = static_cast<const uint8_t *>(packet.Data());
    Require(std::equal(expected.begin(), expected.end(), actual), "payload mismatch");
}

void TestRoundTrip(Packet::Type type, uint8_t aid) {
    const std::vector<uint8_t> payload {0x10, 0x20, 0x30, 0x40};
    constexpr uint64_t Timestamp = UINT64_C(0x0102030405060708);
    const Packet original(payload.data(), payload.size(), Timestamp, aid, type);

    const std::vector<uint8_t> encoded = original.ToBuf();
    const std::optional<Packet> parsed = ParsePacket(encoded.data(), encoded.size());

    Require(parsed.has_value(), "valid packet was rejected");
    Require(parsed->Timestamp() == Timestamp, "timestamp mismatch");
    Require(parsed->Aid() == aid, "aid mismatch");
    Require(parsed->PacketType() == type, "packet type mismatch");
    RequirePayload(*parsed, payload);
}

void TestWireFormat() {
    const std::array<uint8_t, 1> payload {0xAA};
    const Packet packet(
        payload.data(),
        payload.size(),
        UINT64_C(0x0102030405060708),
        7,
        Packet::Type::Reply);
    const std::vector<uint8_t> encoded = packet.ToBuf();
    const std::array<uint8_t, HeaderSize + 1> expected {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x07, 0x01, 0xAA
    };

    Require(encoded.size() == expected.size(), "wire format length mismatch");
    Require(std::equal(expected.begin(), expected.end(), encoded.begin()), "wire format mismatch");
}

void TestUnalignedInput() {
    const std::array<uint8_t, 3> payload {0xA1, 0xB2, 0xC3};
    const Packet original(
        payload.data(),
        payload.size(),
        UINT64_C(0x8877665544332211),
        4,
        Packet::Type::Reply);
    const std::vector<uint8_t> encoded = original.ToBuf();

    std::vector<uint8_t> unaligned(encoded.size() + 1);
    std::copy(encoded.begin(), encoded.end(), unaligned.begin() + 1);
    const std::optional<Packet> parsed = ParsePacket(unaligned.data() + 1, encoded.size());

    Require(parsed.has_value(), "unaligned packet was rejected");
    Require(parsed->Timestamp() == UINT64_C(0x8877665544332211), "unaligned timestamp mismatch");
    Require(parsed->PacketType() == Packet::Type::Reply, "unaligned packet type mismatch");
}

void TestInvalidInput() {
    std::array<uint8_t, HeaderSize> header {};

    Require(!ParsePacket(nullptr, HeaderSize).has_value(), "null input was accepted");
    for (size_t len = 0; len < HeaderSize; ++len) {
        Require(!ParsePacket(header.data(), len).has_value(), "short packet was accepted");
    }

    header[9] = 3;
    Require(!ParsePacket(header.data(), header.size()).has_value(), "invalid packet type was accepted");

    header[9] = 1;
    header[8] = 0;
    Require(!ParsePacket(header.data(), header.size()).has_value(), "reply aid 0 was accepted");
    header[8] = 1;
    Require(ParsePacket(header.data(), header.size()).has_value(), "reply aid 1 was rejected");
    header[8] = 15;
    Require(ParsePacket(header.data(), header.size()).has_value(), "reply aid 15 was rejected");
    header[8] = 16;
    Require(!ParsePacket(header.data(), header.size()).has_value(), "reply aid 16 was accepted");
    header[8] = 255;
    Require(!ParsePacket(header.data(), header.size()).has_value(), "reply aid 255 was accepted");
}

void TestPayloadLimit() {
    std::array<uint8_t, HeaderSize> empty {};
    Require(ParsePacket(empty.data(), empty.size()).has_value(), "empty payload was rejected");

    std::vector<uint8_t> maximum(HeaderSize + MaxPacketPayloadSize);
    maximum[9] = 0;
    Require(ParsePacket(maximum.data(), maximum.size()).has_value(), "maximum payload was rejected");

    maximum.push_back(0);
    Require(!ParsePacket(maximum.data(), maximum.size()).has_value(), "oversized payload was accepted");
}
}

int main() {
    TestRoundTrip(Packet::Type::Other, 0);
    TestRoundTrip(Packet::Type::Reply, 7);
    TestRoundTrip(Packet::Type::Cmd, 0);
    TestWireFormat();
    TestUnalignedInput();
    TestInvalidInput();
    TestPayloadLimit();
    return EXIT_SUCCESS;
}
