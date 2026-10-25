#include "aegisguard/server_config.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace aegisguard {
namespace {

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
        "tls_certificate": "secrets/server.pem",
        "tls_private_key": "secrets/server.key",
        "admin_ca_certificate": "secrets/ca.pem",
        "token_file": "secrets/player.token",
        "maximum_connections": 4
    })");

    EXPECT_EQ(config.port, 18443U);
    EXPECT_EQ(config.admin_port, 8443U);
    EXPECT_EQ(config.tls_certificate, "secrets/server.pem");
    EXPECT_EQ(config.tls_private_key, "secrets/server.key");
    EXPECT_EQ(config.admin_ca_certificate, "secrets/ca.pem");
    EXPECT_EQ(config.token_file, "secrets/player.token");
    EXPECT_EQ(config.maximum_connections, 4U);
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
}

}  // namespace
}  // namespace aegisguard