#pragma once

#include "aegisguard/session_store.hpp"
#include "aegisguard/simulation.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <system_error>

namespace aegisguard {

struct NetworkServiceConfig {
    asio::ip::tcp::endpoint endpoint{
        asio::ip::address_v4::loopback(), 0};
    std::size_t maximum_connections{128};
    std::chrono::milliseconds authentication_timeout{3000};
    std::chrono::milliseconds frame_timeout{1000};
    double movement_rate_capacity{16.0};
    double movement_rate_per_second{60.0};
    std::function<void(std::error_code)> on_error;
};

class NetworkService {
public:
    NetworkService(
        asio::io_context& io_context,
        asio::ssl::context& tls_context,
        SessionStore& sessions,
        Simulation& simulation,
        NetworkServiceConfig config);
    ~NetworkService();

    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;
    NetworkService(NetworkService&&) = delete;
    NetworkService& operator=(NetworkService&&) = delete;

    void start();
    void stop();
    [[nodiscard]] asio::ip::tcp::endpoint local_endpoint() const;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace aegisguard
