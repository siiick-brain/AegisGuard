#include "aegisguard/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data, std::size_t size) {
    if (size > 4096) {
        return 0;
    }
    const auto bytes = std::span<const std::uint8_t>(data, size);
    static_cast<void>(aegisguard::decode_protocol_header(bytes));
    static_cast<void>(aegisguard::decode_move_frame(bytes));
    return 0;
}
