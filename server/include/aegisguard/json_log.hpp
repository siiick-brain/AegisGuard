#pragma once

#include <chrono>
#include <iostream>
#include <ostream>
#include <string_view>
#include <syncstream>

namespace aegisguard {

inline void write_json_string(std::ostream& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    for (const char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                const auto byte = static_cast<unsigned char>(character);
                output << "\\u00" << hex[byte >> 4U] << hex[byte & 0x0fU];
            } else {
                output << character;
            }
        }
    }
}

inline void write_json_log(
    std::string_view level,
    std::string_view event,
    std::string_view message = {}) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::osyncstream output(std::clog);
    output << "{\"timestamp_ms\":" << timestamp << ",\"level\":\"";
    write_json_string(output, level);
    output << "\",\"event\":\"";
    write_json_string(output, event);
    output << "\",\"message\":\"";
    write_json_string(output, message);
    output << "\"}\n";
}

}  // namespace aegisguard
