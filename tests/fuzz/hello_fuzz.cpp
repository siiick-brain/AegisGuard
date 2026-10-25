#include "aegisguard/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data, std::size_t size) {
    if (size > 4096) {
        return 0;
    }
    static_cast<void>(aegisguard::decode_hello_frame(
        std::span<const std::uint8_t>(data, size)));
    return 0;
}
