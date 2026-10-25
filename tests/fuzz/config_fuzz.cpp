#include "aegisguard/server_config.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data, std::size_t size) {
    if (size > 65536) {
        return 0;
    }
    const auto input = std::string_view(
        reinterpret_cast<const char*>(data), size);
    try {
        static_cast<void>(aegisguard::parse_server_config(input));
    } catch (const std::exception&) {
    }
    return 0;
}
