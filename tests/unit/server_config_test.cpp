#include "aegisguard/server_config.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace aegisguard {
namespace {

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(std::string name, std::string value)
        : name_(std::move(name)) {
        if (const char* const existing = std::getenv(name_.c_str())) {
            original_ = existing;
        }
#ifdef _WIN32
        if (_putenv_s(name_.c_str(), value.c_str()) != 0) {
            throw std::runtime_error("failed to set test environment variable");
        }
#else
        if (setenv(name_.c_str(), value.c_str(), 1) != 0) {
            throw std::runtime_error("failed to set test environment variable");
        }
#endif
    }

    ~ScopedEnvironmentVariable() {
#ifdef _WIN32
        static_cast<void>(
            _putenv_s(name_.c_str(), original_ ? original_->c_str() : ""));
#else
        if (original_) {
            static_cast<void>(setenv(name_.c_str(), original_->c_str(), 1));
        } else {
            static_cast<void>(unsetenv(name_.c_str()));
        }
#endif
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

private:
    std::string name_;
    std::optional<std::string> original_;
};

TEST(ServerConfigTest, DefaultsToLoopback) {
    const ServerConfig config = parse_server_config("{}");

    EXPECT_EQ(config.bind_address, "127.0.0.1");
    EXPECT_EQ(config.port, 8443U);
    EXPECT_EQ(config.maximum_connections, 128U);
}

TEST(ServerConfigTest, ParsesPortAndProvisioningPaths) {
    const ServerConfig config = parse_server_config(R"({
        "bind_address": "127.0.0.1",
        "port": 18443,
        "admin_port": 8443,
        "tls_certificate": "fixture/server.pem",
        "tls_private_key": "fixture/server.key",
        "admin_ca_certificate": "fixture/ca.pem",
        "token_file": "fixture/player.token",
        "maximum_connections": 4
    })");

    EXPECT_EQ(config.port, 18443U);
    EXPECT_EQ(config.admin_port, 8443U);
    EXPECT_EQ(config.tls_certificate, "fixture/server.pem");
    EXPECT_EQ(config.tls_private_key, "fixture/server.key");
    EXPECT_EQ(config.admin_ca_certificate, "fixture/ca.pem");
    EXPECT_EQ(config.token_file, "fixture/player.token");
    EXPECT_EQ(config.maximum_connections, 4U);
}

TEST(ServerConfigTest, ExpandsLocalAppDataForProvisioningPaths) {
    ScopedEnvironmentVariable local_app_data{
        "LOCALAPPDATA", "C:/Users/test/AppData/Local"};
    const ServerConfig config = parse_server_config(R"({
        "tls_certificate": "${LOCALAPPDATA}/AegisGuard/secrets/server.pem",
        "tls_private_key": "${LOCALAPPDATA}/AegisGuard/secrets/server.key",
        "admin_ca_certificate": "${LOCALAPPDATA}/AegisGuard/secrets/ca.pem",
        "token_file": "${LOCALAPPDATA}/AegisGuard/secrets/player.token"
    })");

    EXPECT_EQ(config.tls_certificate,
              "C:/Users/test/AppData/Local/AegisGuard/secrets/server.pem");
    EXPECT_EQ(config.tls_private_key,
              "C:/Users/test/AppData/Local/AegisGuard/secrets/server.key");
    EXPECT_EQ(config.admin_ca_certificate,
              "C:/Users/test/AppData/Local/AegisGuard/secrets/ca.pem");
    EXPECT_EQ(config.token_file,
              "C:/Users/test/AppData/Local/AegisGuard/secrets/player.token");
}

TEST(ServerConfigTest, RejectsUnsetEnvironmentVariableInProvisioningPath) {
    ScopedEnvironmentVariable unset_variable{
        "AEGISGUARD_TEST_UNSET_PATH", ""};

    EXPECT_THROW(
        static_cast<void>(parse_server_config(
            R"({"tls_certificate": "${AEGISGUARD_TEST_UNSET_PATH}/server.pem"})")),
        std::invalid_argument);
}

TEST(ServerConfigTest, ParsesQueueAndPerIpLimits) {
    const ServerConfig config = parse_server_config(R"({
        "maximum_queued_intents_per_player": 12,
        "ip_rate_capacity": 90,
        "ip_rate_per_second": 45,
        "maximum_tracked_ip_rates": 200
    })");

    EXPECT_EQ(config.maximum_queued_intents_per_player, 12U);
    EXPECT_DOUBLE_EQ(config.ip_rate_capacity, 90.0);
    EXPECT_DOUBLE_EQ(config.ip_rate_per_second, 45.0);
    EXPECT_EQ(config.maximum_tracked_ip_rates, 200U);
}

TEST(ServerConfigTest, RejectsInvalidDocumentAndPort) {
    EXPECT_THROW(
        static_cast<void>(parse_server_config("{broken}")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_server_config(R"({"port": 0})")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_server_config(R"({"port": 65536})")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_server_config(
            R"({"port": 18443, "admin_port": 18443})")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_server_config(
            R"({"maximum_queued_intents_per_player": 0})")),
        std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(parse_server_config(R"({"ip_rate_capacity": 0})")),
        std::invalid_argument);
}

}  // namespace
}  // namespace aegisguard