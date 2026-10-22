#pragma once

#include <asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aegisguard {

inline constexpr std::size_t kProtocolHeaderSize = 14;
inline constexpr std::size_t kMovePayloadSize = 4;
inline constexpr std::size_t kMaximumFrameSize = kProtocolHeaderSize + kMovePayloadSize;

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
};

struct DecodeResult {
    ProtocolError error{ProtocolError::none};
    MoveFrame frame{};
};

[[nodiscard]] DecodeResult decode_move_frame(std::span<const std::uint8_t> bytes) noexcept;

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
    if (payload_size > kMovePayloadSize) {
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
