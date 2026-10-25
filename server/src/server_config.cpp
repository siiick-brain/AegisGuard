#include "aegisguard/server_config.hpp"

#include "aegisguard/simulation.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace aegisguard {
namespace {

struct JsonValue {
    std::string text;
    bool is_string{};
};

class JsonObjectParser {
public:
    explicit JsonObjectParser(std::string_view input) : input_(input) {}

    std::map<std::string, JsonValue> parse() {
        skip_whitespace();
        expect('{');
        skip_whitespace();

        std::map<std::string, JsonValue> fields;
        if (consume('}')) {
            finish();
            return fields;
        }

        while (true) {
            skip_whitespace();
            std::string key = parse_string();
            skip_whitespace();
            expect(':');
            skip_whitespace();
            JsonValue value = parse_value();
            if (!fields.emplace(std::move(key), std::move(value)).second) {
                throw std::invalid_argument("duplicate config key");
            }

            skip_whitespace();
            if (consume('}')) {
                break;
            }
            expect(',');
        }

        finish();
        return fields;
    }

private:
    std::string_view input_;
    std::size_t position_{};

    void skip_whitespace() {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
                return;
            }
            ++position_;
        }
    }

    void expect(char expected) {
        if (!consume(expected)) {
            throw std::invalid_argument("invalid JSON config syntax");
        }
    }

    bool consume(char expected) {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    static int hex_value(char value) {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    }

    std::uint32_t parse_hex_quad() {
        if (input_.size() - position_ < 4) {
            throw std::invalid_argument("incomplete Unicode escape in config");
        }
        std::uint32_t result = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const int digit = hex_value(input_[position_++]);
            if (digit < 0) {
                throw std::invalid_argument("invalid Unicode escape in config");
            }
            result = (result << 4U) | static_cast<std::uint32_t>(digit);
        }
        return result;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7FU) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FFU) {
            output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else if (codepoint <= 0xFFFFU) {
            output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        }
    }

    void append_escape(std::string& output) {
        if (position_ >= input_.size()) {
            throw std::invalid_argument("incomplete escape in config string");
        }
        const char escaped = input_[position_++];
        switch (escaped) {
            case '"': output.push_back('"'); return;
            case '\\': output.push_back('\\'); return;
            case '/': output.push_back('/'); return;
            case 'b': output.push_back('\b'); return;
            case 'f': output.push_back('\f'); return;
            case 'n': output.push_back('\n'); return;
            case 'r': output.push_back('\r'); return;
            case 't': output.push_back('\t'); return;
            case 'u': {
                std::uint32_t codepoint = parse_hex_quad();
                if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
                    if (input_.size() - position_ < 6 ||
                        input_[position_] != '\\' || input_[position_ + 1] != 'u') {
                        throw std::invalid_argument("invalid Unicode surrogate in config");
                    }
                    position_ += 2;
                    const std::uint32_t low = parse_hex_quad();
                    if (low < 0xDC00U || low > 0xDFFFU) {
                        throw std::invalid_argument("invalid Unicode surrogate in config");
                    }
                    codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) +
                                (low - 0xDC00U);
                } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
                    throw std::invalid_argument("invalid Unicode surrogate in config");
                }
                append_utf8(output, codepoint);
                return;
            }
            default:
                throw std::invalid_argument("invalid escape in config string");
        }
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (position_ < input_.size()) {
            const unsigned char value = static_cast<unsigned char>(input_[position_++]);
            if (value == '"') {
                return result;
            }
            if (value == '\\') {
                append_escape(result);
            } else if (value < 0x20U) {
                throw std::invalid_argument("control character in config string");
            } else {
                result.push_back(static_cast<char>(value));
            }
        }
        throw std::invalid_argument("unterminated config string");
    }

    JsonValue parse_value() {
        if (position_ >= input_.size()) {
            throw std::invalid_argument("missing config value");
        }
        if (input_[position_] == '"') {
            return {parse_string(), true};
        }

        const std::size_t start = position_;
        if (consume('-') && position_ >= input_.size()) {
            throw std::invalid_argument("invalid number in config");
        }
        if (consume('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' &&
                input_[position_] <= '9') {
                throw std::invalid_argument("leading zero in config number");
            }
        } else {
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
            if (position_ == digits) {
                throw std::invalid_argument("invalid config value");
            }
        }
        if (consume('.')) {
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
            if (position_ == digits) {
                throw std::invalid_argument("invalid fraction in config number");
            }
        }
        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            const std::size_t digits = position_;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                ++position_;
            }
            if (position_ == digits) {
                throw std::invalid_argument("invalid exponent in config number");
            }
        }
        return {std::string(input_.substr(start, position_ - start)), false};
    }

    void finish() {
        skip_whitespace();
        if (position_ != input_.size()) {
            throw std::invalid_argument("trailing data after JSON config");
        }
    }
};

JsonValue take_value(std::map<std::string, JsonValue>& fields, std::string_view key,
                     JsonValue fallback) {
    const auto found = fields.find(std::string(key));
    if (found == fields.end()) {
        return fallback;
    }
    JsonValue value = std::move(found->second);
    fields.erase(found);
    return value;
}

std::string take_string(std::map<std::string, JsonValue>& fields,
                        std::string_view key, std::string fallback = {}) {
    JsonValue value = take_value(fields, key, {std::move(fallback), true});
    if (!value.is_string) {
        throw std::invalid_argument("config value must be a string: " + std::string(key));
    }
    return value.text;
}

std::uint64_t parse_unsigned(const JsonValue& value, std::string_view key) {
    if (value.is_string || value.text.empty() || value.text.front() == '-') {
        throw std::invalid_argument("config value must be an unsigned integer: " +
                                    std::string(key));
    }
    std::uint64_t result{};
    const auto [end, error] = std::from_chars(
        value.text.data(), value.text.data() + value.text.size(), result);
    if (error != std::errc{} || end != value.text.data() + value.text.size()) {
        throw std::invalid_argument("invalid unsigned integer in config: " +
                                    std::string(key));
    }
    return result;
}

std::uint64_t take_unsigned(std::map<std::string, JsonValue>& fields,
                            std::string_view key, std::uint64_t fallback) {
    return parse_unsigned(take_value(fields, key, {std::to_string(fallback), false}), key);
}

double parse_number(const JsonValue& value, std::string_view key) {
    if (value.is_string || value.text.empty()) {
        throw std::invalid_argument("config value must be numeric: " + std::string(key));
    }
    double result{};
    const auto [end, error] = std::from_chars(
        value.text.data(), value.text.data() + value.text.size(), result,
        std::chars_format::general);
    if (error != std::errc{} || end != value.text.data() + value.text.size() ||
        !std::isfinite(result)) {
        throw std::invalid_argument("invalid number in config: " + std::string(key));
    }
    return result;
}

double take_number(std::map<std::string, JsonValue>& fields,
                   std::string_view key, double fallback) {
    return parse_number(take_value(fields, key, {std::to_string(fallback), false}), key);
}

std::size_t bounded_size(std::uint64_t value, std::string_view key) {
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("config value is too large: " + std::string(key));
    }
    return static_cast<std::size_t>(value);
}

std::chrono::milliseconds bounded_milliseconds(std::uint64_t value,
                                                std::string_view key) {
    using Rep = std::chrono::milliseconds::rep;
    if (value > static_cast<std::uint64_t>(std::numeric_limits<Rep>::max())) {
        throw std::invalid_argument("config duration is too large: " + std::string(key));
    }
    return std::chrono::milliseconds{static_cast<Rep>(value)};
}

}  // namespace

ServerConfig parse_server_config(std::string_view json) {
    auto fields = JsonObjectParser(json).parse();
    ServerConfig config;
    config.bind_address = take_string(fields, "bind_address", config.bind_address);

    const std::uint64_t port = take_unsigned(fields, "port", config.port);
    if (port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("port must be between 1 and 65535");
    }
    config.port = static_cast<std::uint16_t>(port);
    const std::uint64_t admin_port = take_unsigned(fields, "admin_port", config.admin_port);
    if (admin_port == 0 || admin_port > std::numeric_limits<std::uint16_t>::max() ||
        admin_port == port) {
        throw std::invalid_argument("admin_port must be a valid port distinct from port");
    }
    config.admin_port = static_cast<std::uint16_t>(admin_port);
    config.simulation.world_width =
        take_number(fields, "world_width", config.simulation.world_width);
    config.simulation.world_height =
        take_number(fields, "world_height", config.simulation.world_height);
    config.simulation.max_speed =
        take_number(fields, "max_speed", config.simulation.max_speed);
    config.simulation.fixed_delta_seconds = take_number(
        fields, "fixed_delta_seconds", config.simulation.fixed_delta_seconds);
    config.maximum_connections = bounded_size(
        take_unsigned(fields, "maximum_connections", config.maximum_connections),
        "maximum_connections");
    config.authentication_timeout = bounded_milliseconds(
        take_unsigned(fields, "authentication_timeout_ms",
                      static_cast<std::uint64_t>(config.authentication_timeout.count())),
        "authentication_timeout_ms");
    config.frame_timeout = bounded_milliseconds(
        take_unsigned(fields, "frame_timeout_ms",
                      static_cast<std::uint64_t>(config.frame_timeout.count())),
        "frame_timeout_ms");
    config.movement_rate_capacity =
        take_number(fields, "movement_rate_capacity", config.movement_rate_capacity);
    config.movement_rate_per_second = take_number(
        fields, "movement_rate_per_second", config.movement_rate_per_second);
    config.maximum_sessions = bounded_size(
        take_unsigned(fields, "maximum_sessions", config.maximum_sessions),
        "maximum_sessions");
    config.tls_certificate = take_string(fields, "tls_certificate");
    config.tls_private_key = take_string(fields, "tls_private_key");
    config.admin_ca_certificate = take_string(fields, "admin_ca_certificate");
    config.token_file = take_string(fields, "token_file");
    config.demo_player_id = take_unsigned(fields, "demo_player_id", config.demo_player_id);
    config.initial_position.x = take_number(fields, "initial_x", config.initial_position.x);
    config.initial_position.y = take_number(fields, "initial_y", config.initial_position.y);

    if (!fields.empty()) {
        throw std::invalid_argument("unknown key in server config: " + fields.begin()->first);
    }
    if (config.bind_address.empty() || config.maximum_connections == 0 ||
        config.authentication_timeout <= std::chrono::milliseconds::zero() ||
        config.frame_timeout <= std::chrono::milliseconds::zero() ||
        config.maximum_sessions == 0 || config.demo_player_id == 0 ||
        !std::isfinite(config.movement_rate_capacity) ||
        config.movement_rate_capacity < 1.0 ||
        !std::isfinite(config.movement_rate_per_second) ||
        config.movement_rate_per_second <= 0.0) {
        throw std::invalid_argument("server config values are outside supported limits");
    }
    Simulation simulation(config.simulation);
    if (simulation.add_player(config.demo_player_id, config.initial_position) !=
        SimulationError::none) {
        throw std::invalid_argument("initial player position is outside the world");
    }
    return config;
}

}  // namespace aegisguard