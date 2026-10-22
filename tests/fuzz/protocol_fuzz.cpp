#include "aegisguard/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data, std::size_t size) {
    static_cast<void>(
        aegisguard::decode_move_frame(std::span<const std::uint8_t>(data, size)));
    return 0;
}
