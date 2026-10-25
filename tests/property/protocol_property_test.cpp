#include "aegisguard/protocol.hpp"

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace aegisguard {
namespace {

void write_u16(std::uint8_t* output, std::uint16_t value) {
    output[0] = static_cast<std::uint8_t>(value >> 8U);
    output[1] = static_cast<std::uint8_t>(value);
}

void write_u32(std::uint8_t* output, std::uint32_t value) {
    output[0] = static_cast<std::uint8_t>(value >> 24U);
    output[1] = static_cast<std::uint8_t>(value >> 16U);
    output[2] = static_cast<std::uint8_t>(value >> 8U);
    output[3] = static_cast<std::uint8_t>(value);
}

std::array<std::uint8_t, kMaximumFrameSize> encode_for_test(
    const MoveFrame& frame, std::int16_t x, std::int16_t y,
    std::uint16_t angle, std::int32_t position_x,
    std::int32_t position_y) {
    std::array<std::uint8_t, kMaximumFrameSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = kProtocolVersion;
    bytes[3] = kMoveMessageType;
    write_u16(bytes.data() + 4, static_cast<std::uint16_t>(kMovePayloadSize));
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned int>((7 - index) * 8);
        bytes[6 + index] = static_cast<std::uint8_t>(frame.sequence >> shift);
    }
    write_u16(bytes.data() + 14, static_cast<std::uint16_t>(x));
    write_u16(bytes.data() + 16, static_cast<std::uint16_t>(y));
    write_u16(bytes.data() + 18, angle);
    write_u32(bytes.data() + 20, static_cast<std::uint32_t>(position_x));
    write_u32(bytes.data() + 24, static_cast<std::uint32_t>(position_y));
    return bytes;
}

RC_GTEST_PROP(ProtocolProperty, MovementFramesRoundTripQuantizedValues, ()) {
    MoveFrame input;
    input.sequence = *rc::gen::inRange<std::uint64_t>(
        1, std::numeric_limits<std::uint64_t>::max());
    const auto x = static_cast<std::int16_t>(*rc::gen::inRange(-1000, 1001));
    const auto y = static_cast<std::int16_t>(*rc::gen::inRange(-1000, 1001));
    const auto angle = static_cast<std::uint16_t>(
        *rc::gen::arbitrary<std::uint16_t>());
    const auto position_x = static_cast<std::int32_t>(
        *rc::gen::inRange(-100000, 100001));
    const auto position_y = static_cast<std::int32_t>(
        *rc::gen::inRange(-100000, 100001));
    const auto bytes = encode_for_test(
        input, x, y, angle, position_x, position_y);

    const auto decoded = decode_move_frame(bytes);
    RC_ASSERT(decoded.error == ProtocolError::none);
    RC_ASSERT(decoded.frame.sequence == input.sequence);
    RC_ASSERT(decoded.frame.x == static_cast<double>(x) / 1000.0);
    RC_ASSERT(decoded.frame.y == static_cast<double>(y) / 1000.0);
    RC_ASSERT(decoded.frame.reported_position_x ==
              static_cast<double>(position_x) / 1000.0);
    RC_ASSERT(decoded.frame.reported_position_y ==
              static_cast<double>(position_y) / 1000.0);
    RC_ASSERT(std::isfinite(decoded.frame.aim_angle_radians));
}

RC_GTEST_PROP(ProtocolProperty, ArbitraryBytesNeverCrashAnyDecoder, ()) {
    const auto bytes = *rc::gen::container<std::vector<std::uint8_t>>(
        rc::gen::arbitrary<std::uint8_t>());
    static_cast<void>(decode_protocol_header(bytes));
    static_cast<void>(decode_move_frame(bytes));
    static_cast<void>(decode_fire_frame(bytes));
    static_cast<void>(decode_hello_frame(bytes));
}

RC_GTEST_PROP(ProtocolProperty, ExplicitHelloFramesRoundTripVersion, ()) {
    std::array<std::uint8_t, kHelloFrameSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = kProtocolVersion;
    bytes[3] = kHelloMessageType;
    write_u16(bytes.data() + 4, static_cast<std::uint16_t>(kHelloPayloadSize));
    write_u16(bytes.data() + kProtocolHeaderSize, kProtocolVersion);

    const auto decoded = decode_hello_frame(bytes);
    RC_ASSERT(decoded.error == ProtocolError::none);
    RC_ASSERT(decoded.frame.protocol_version == kProtocolVersion);
}

RC_GTEST_PROP(ProtocolProperty, SequenceGuardMatchesStrictlyIncreasingModel, ()) {
    SequenceGuard guard;
    std::uint64_t last_accepted = 0;
    const auto sequences = *rc::gen::container<std::vector<std::uint64_t>>(
        rc::gen::arbitrary<std::uint64_t>());
    for (const auto sequence : sequences) {
        ProtocolError expected = ProtocolError::none;
        if (sequence == 0) {
            expected = ProtocolError::invalid_sequence;
        } else if (sequence <= last_accepted) {
            expected = ProtocolError::replay_or_out_of_order;
        } else {
            last_accepted = sequence;
        }
        RC_ASSERT(guard.accept(sequence) == expected);
        RC_ASSERT(guard.last_accepted() == last_accepted);
    }
}

}  // namespace
}  // namespace aegisguard
