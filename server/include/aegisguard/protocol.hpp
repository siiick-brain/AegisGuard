#pragma once

#include <asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aegisguard {

inline constexpr std::uint8_t kProtocolVersion = 3;
inline constexpr std::size_t kProtocolHeaderSize = 14;
inline constexpr std::size_t kHelloPayloadSize = 2;
inline constexpr std::size_t kMovePayloadSize = 14;
inline constexpr std::size_t kFirePayloadSize = 10;
inline constexpr std::size_t kMaximumPayloadSize =
    kMovePayloadSize > kFirePayloadSize ? kMovePayloadSize : kFirePayloadSize;
inline constexpr std::size_t kMaximumFrameSize =
    kProtocolHeaderSize + kMaximumPayloadSize;
inline constexpr std::size_t kHelloFrameSize =
    kProtocolHeaderSize + kHelloPayloadSize;
inline constexpr std::uint8_t kHelloMessageType = 0;
inline constexpr std::uint8_t kMoveMessageType = 1;
inline constexpr std::uint8_t kFireMessageType = 2;

enum class ProtocolError {
    none,
    incomplete_header,
    invalid_magic,
    unsupported_version,
    unknown_message_type,
    payload_too_large,
    invalid_payload_length,
    incomplete_frame,
    trailing_bytes,
    invalid_sequence,
    invalid_movement,
    replay_or_out_of_order,
};

struct MoveFrame {
    std::uint64_t sequence{};
    double x{};
    double y{};
    double aim_angle_radians{};
    double reported_position_x{};
    double reported_position_y{};
};

struct FireFrame {
    std::uint64_t sequence{};
    double aim_angle_radians{};
    std::uint64_t tick_reference{};
};

struct ProtocolHeader {
    ProtocolError error{ProtocolError::none};
    std::uint8_t message_type{};
    std::uint16_t payload_size{};
    std::uint64_t sequence{};
};

struct HelloFrame {
    std::uint16_t protocol_version{};
};

struct HelloDecodeResult {
    ProtocolError error{ProtocolError::none};
    HelloFrame frame{};
};

struct DecodeResult {
    ProtocolError error{ProtocolError::none};
    MoveFrame frame{};
};

struct FireDecodeResult {
    ProtocolError error{ProtocolError::none};
    FireFrame frame{};
};

[[nodiscard]] DecodeResult decode_move_frame(std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] FireDecodeResult decode_fire_frame(
    std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] ProtocolHeader decode_protocol_header(
    std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] HelloDecodeResult decode_hello_frame(
    std::span<const std::uint8_t> bytes) noexcept;

template <typename SyncReadStream>
[[nodiscard]] DecodeResult read_move_frame(SyncReadStream& stream) {
    std::array<std::uint8_t, kProtocolHeaderSize> header{};
    asio::read(stream, asio::buffer(header));

    const DecodeResult header_result = decode_move_frame(header);
    if (header_result.error != ProtocolError::incomplete_frame) {
        return header_result;
    }

    const std::size_t payload_size =
        (static_cast<std::size_t>(header[4]) << 8U) | static_cast<std::size_t>(header[5]);
    if (payload_size > kMaximumPayloadSize) {
        return {ProtocolError::payload_too_large, {}};
    }
    if (payload_size != kMovePayloadSize) {
        return {ProtocolError::invalid_payload_length, {}};
    }

    std::array<std::uint8_t, kMaximumFrameSize> bytes{};
    std::copy(header.begin(), header.end(), bytes.begin());
    asio::read(stream, asio::buffer(bytes.data() + kProtocolHeaderSize, payload_size));
    return decode_move_frame(bytes);
}

class SequenceGuard {
public:
    [[nodiscard]] ProtocolError accept(std::uint64_t sequence) noexcept;
    [[nodiscard]] std::uint64_t last_accepted() const noexcept;

private:
    std::uint64_t last_accepted_{};
};

class TokenBucket {
public:
    using Clock = std::chrono::steady_clock;

    TokenBucket(double capacity, double tokens_per_second, Clock::time_point now);

    [[nodiscard]] bool consume(Clock::time_point now);
    [[nodiscard]] double available_tokens() const noexcept;

private:
    double capacity_;
    double tokens_per_second_;
    double tokens_;
    Clock::time_point last_refill_;
};

}  // namespace aegisguard
