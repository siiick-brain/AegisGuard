#include "aegisguard/protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace aegisguard {
namespace {

constexpr std::uint8_t kMagicFirst = 0x41;
constexpr std::uint8_t kMagicSecond = 0x47;
constexpr std::uint8_t kSchemaVersion = 1;
constexpr std::uint8_t kMoveMessageType = 1;
constexpr double kMovementScale = 1000.0;

std::uint16_t read_u16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8U) |
        static_cast<std::uint16_t>(bytes[1]));
}

std::uint64_t read_u64(const std::uint8_t* bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        value = (value << 8U) | bytes[index];
    }
    return value;
}

std::int32_t read_i16(const std::uint8_t* bytes) noexcept {
    const std::uint16_t raw = read_u16(bytes);
    if (raw <= static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max())) {
        return raw;
    }
    return static_cast<std::int32_t>(raw) - 65536;
}

}  // namespace

DecodeResult decode_move_frame(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < kProtocolHeaderSize) {
        return {ProtocolError::incomplete_header, {}};
    }
    if (bytes[0] != kMagicFirst || bytes[1] != kMagicSecond) {
        return {ProtocolError::invalid_magic, {}};
    }
    if (bytes[2] != kSchemaVersion) {
        return {ProtocolError::unsupported_version, {}};
    }
    if (bytes[3] != kMoveMessageType) {
        return {ProtocolError::unknown_message_type, {}};
    }

    const std::size_t payload_size = read_u16(bytes.data() + 4);
    if (payload_size > kMovePayloadSize) {
        return {ProtocolError::payload_too_large, {}};
    }
    if (payload_size != kMovePayloadSize) {
        return {ProtocolError::invalid_payload_length, {}};
    }
    if (bytes.size() < kMaximumFrameSize) {
        return {ProtocolError::incomplete_frame, {}};
    }
    if (bytes.size() > kMaximumFrameSize) {
        return {ProtocolError::trailing_bytes, {}};
    }

    const std::uint64_t sequence = read_u64(bytes.data() + 6);
    if (sequence == 0) {
        return {ProtocolError::invalid_sequence, {}};
    }

    const std::int32_t x = read_i16(bytes.data() + kProtocolHeaderSize);
    const std::int32_t y = read_i16(bytes.data() + kProtocolHeaderSize + 2);
    if (std::abs(x) > 1000 || std::abs(y) > 1000) {
        return {ProtocolError::invalid_movement, {}};
    }

    return {
        ProtocolError::none,
        MoveFrame{sequence, static_cast<double>(x) / kMovementScale,
                  static_cast<double>(y) / kMovementScale},
    };
}

ProtocolError SequenceGuard::accept(std::uint64_t sequence) noexcept {
    if (sequence == 0) {
        return ProtocolError::invalid_sequence;
    }
    if (sequence <= last_accepted_) {
        return ProtocolError::replay_or_out_of_order;
    }
    last_accepted_ = sequence;
    return ProtocolError::none;
}

std::uint64_t SequenceGuard::last_accepted() const noexcept {
    return last_accepted_;
}

TokenBucket::TokenBucket(double capacity, double tokens_per_second, Clock::time_point now)
    : capacity_(capacity),
      tokens_per_second_(tokens_per_second),
      tokens_(capacity),
      last_refill_(now) {
    if (!std::isfinite(capacity_) || capacity_ < 1.0 ||
        !std::isfinite(tokens_per_second_) || tokens_per_second_ <= 0.0) {
        throw std::invalid_argument("rate limit capacity and refill rate must be finite and positive");
    }
}

bool TokenBucket::consume(Clock::time_point now) {
    if (now < last_refill_) {
        return false;
    }

    const double elapsed = std::chrono::duration<double>(now - last_refill_).count();
    tokens_ = std::min(capacity_, tokens_ + elapsed * tokens_per_second_);
    last_refill_ = now;
    if (tokens_ < 1.0) {
        return false;
    }

    tokens_ -= 1.0;
    return true;
}

double TokenBucket::available_tokens() const noexcept {
    return tokens_;
}

}  // namespace aegisguard
