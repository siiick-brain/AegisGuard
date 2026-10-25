#pragma once

#include "aegisguard/session_store.hpp"
#include "aegisguard/movement_anomaly.hpp"
#include "aegisguard/rule_detector.hpp"
#include "aegisguard/simulation.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
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
    double ip_rate_capacity{128.0};
    double ip_rate_per_second{120.0};
    std::size_t maximum_tracked_ip_rates{4096};
    std::size_t maximum_queued_intents_per_player{64};
    MovementAnomalyConfig movement_anomaly{};
    RuleDetectorConfig rule_detector{};
    std::function<void(std::error_code)> on_error;
    std::function<void(const RuleFinding&)> on_detection;
    std::function<void(std::string_view, const void*)> on_lifecycle;
};

class NetworkService {
public:
    NetworkService(
        std::shared_ptr<asio::io_context> io_context,
        std::shared_ptr<asio::ssl::context> tls_context,
        SessionStore& sessions,
        Simulation& simulation,
        NetworkServiceConfig config);
    ~NetworkService();

    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;
    NetworkService(NetworkService&&) = delete;
    NetworkService& operator=(NetworkService&&) = delete;

    void start();
    // Keep the shared io_context running until run() drains cancelled handlers.
    void stop();
    [[nodiscard]] asio::ip::tcp::endpoint local_endpoint() const;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace aegisguard
