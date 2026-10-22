#include "aegisguard/protocol.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aegisguard {
namespace {

using Clock = TokenBucket::Clock;

std::array<std::uint8_t, kMaximumFrameSize> make_frame(
    std::uint64_t sequence, std::int16_t x, std::int16_t y) {
    std::array<std::uint8_t, kMaximumFrameSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = 1;
    bytes[3] = 1;
    bytes[4] = 0;
    bytes[5] = static_cast<std::uint8_t>(kMovePayloadSize);
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned int>((7 - index) * 8);
        bytes[6 + index] = static_cast<std::uint8_t>(sequence >> shift);
    }

    const auto x_bits = static_cast<std::uint16_t>(x);
    const auto y_bits = static_cast<std::uint16_t>(y);
    bytes[14] = static_cast<std::uint8_t>(x_bits >> 8U);
    bytes[15] = static_cast<std::uint8_t>(x_bits);
    bytes[16] = static_cast<std::uint8_t>(y_bits >> 8U);
    bytes[17] = static_cast<std::uint8_t>(y_bits);
    return bytes;
}

TEST(ProtocolTest, DecodesVersionOneMovementFrame) {
    const auto bytes = make_frame(42, -1000, 500);

    const auto result = decode_move_frame(bytes);

    ASSERT_EQ(result.error, ProtocolError::none);
    EXPECT_EQ(result.frame.sequence, 42U);
    EXPECT_DOUBLE_EQ(result.frame.x, -1.0);
    EXPECT_DOUBLE_EQ(result.frame.y, 0.5);
}

TEST(ProtocolTest, RejectsMalformedHeadersAndLengths) {
    auto bytes = make_frame(1, 0, 0);
    EXPECT_EQ(decode_move_frame(std::span(bytes).first(5)).error,
              ProtocolError::incomplete_header);

    bytes[0] = 0;
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::invalid_magic);
    bytes[0] = 0x41;
    bytes[2] = 2;
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::unsupported_version);
    bytes[2] = 1;
    bytes[3] = 2;
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::unknown_message_type);
    bytes[3] = 1;
    bytes[5] = static_cast<std::uint8_t>(kMovePayloadSize + 1);
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::payload_too_large);
    bytes[5] = 0;
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::invalid_payload_length);
}

TEST(ProtocolTest, RejectsIncompleteFramesTrailingBytesAndInvalidSequence) {
    auto bytes = make_frame(1, 0, 0);
    EXPECT_EQ(decode_move_frame(std::span(bytes).first(kMaximumFrameSize - 1)).error,
              ProtocolError::incomplete_frame);

    std::vector<std::uint8_t> trailing(bytes.begin(), bytes.end());
    trailing.push_back(0);
    EXPECT_EQ(decode_move_frame(trailing).error, ProtocolError::trailing_bytes);

    bytes = make_frame(0, 0, 0);
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::invalid_sequence);
}

TEST(ProtocolTest, RejectsMovementOutsideNormalizedRange) {
    const auto bytes = make_frame(1, 1001, 0);
    EXPECT_EQ(decode_move_frame(bytes).error, ProtocolError::invalid_movement);
}

TEST(ProtocolTest, AcceptsOnlyStrictlyIncreasingSequences) {
    SequenceGuard guard;
    EXPECT_EQ(guard.accept(0), ProtocolError::invalid_sequence);
    EXPECT_EQ(guard.accept(4), ProtocolError::none);
    EXPECT_EQ(guard.accept(4), ProtocolError::replay_or_out_of_order);
    EXPECT_EQ(guard.accept(3), ProtocolError::replay_or_out_of_order);
    EXPECT_EQ(guard.accept(5), ProtocolError::none);
    EXPECT_EQ(guard.last_accepted(), 5U);
}

TEST(ProtocolTest, ReadsACompleteFrameFromTcpStream) {
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor(
        context, {asio::ip::address_v4::loopback(), 0});
    asio::ip::tcp::socket sender(context);
    asio::ip::tcp::socket receiver(context);
    sender.connect(acceptor.local_endpoint());
    acceptor.accept(receiver);

    const auto bytes = make_frame(8, 250, -250);
    asio::write(sender, asio::buffer(bytes));

    const auto result = read_move_frame(receiver);

    ASSERT_EQ(result.error, ProtocolError::none);
    EXPECT_EQ(result.frame.sequence, 8U);
    EXPECT_DOUBLE_EQ(result.frame.x, 0.25);
    EXPECT_DOUBLE_EQ(result.frame.y, -0.25);
}

TEST(ProtocolTest, EnforcesTokenBucketCapacityAndRefill) {
    const auto start = Clock::time_point{};
    TokenBucket limiter(2.0, 2.0, start);

    EXPECT_TRUE(limiter.consume(start));
    EXPECT_TRUE(limiter.consume(start));
    EXPECT_FALSE(limiter.consume(start));
    EXPECT_TRUE(limiter.consume(start + std::chrono::milliseconds(500)));
    EXPECT_FALSE(limiter.consume(start + std::chrono::milliseconds(500)));
    EXPECT_TRUE(limiter.consume(start + std::chrono::seconds(5)));
    EXPECT_TRUE(limiter.consume(start + std::chrono::seconds(5)));
    EXPECT_FALSE(limiter.consume(start + std::chrono::seconds(5)));
}

TEST(ProtocolTest, RejectsInvalidRateLimitConfigurationAndClockRegression) {
    const auto start = Clock::time_point{};
    EXPECT_THROW(TokenBucket(0.0, 1.0, start), std::invalid_argument);
    EXPECT_THROW(
        TokenBucket(1.0, std::numeric_limits<double>::infinity(), start),
        std::invalid_argument);

    TokenBucket limiter(1.0, 1.0, start);
    EXPECT_FALSE(limiter.consume(start - std::chrono::seconds(1)));
    EXPECT_DOUBLE_EQ(limiter.available_tokens(), 1.0);
}

}  // namespace
}  // namespace aegisguard
