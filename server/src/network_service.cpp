#include "aegisguard/network_service.hpp"

#include "aegisguard/json_log.hpp"
#include "aegisguard/intent_queue.hpp"
#include "aegisguard/ip_rate_limiter.hpp"
#include "aegisguard/protocol.hpp"

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <syncstream>
#include <utility>
#include <vector>

namespace aegisguard {
namespace {

using Tcp = asio::ip::tcp;
using SteadyClock = std::chrono::steady_clock;

constexpr std::size_t kCredentialSize = SessionCredential{}.size();

std::error_code service_error() {
    return std::make_error_code(std::errc::state_not_recoverable);
}

std::optional<RuleSignal> movement_rule_signal(MovementAnomaly anomaly) {
    switch (anomaly) {
        case MovementAnomaly::none:
        case MovementAnomaly::invalid_sample:
        case MovementAnomaly::clock_regression:
            return std::nullopt;
        case MovementAnomaly::speed:
            return RuleSignal::speed_exceeded;
        case MovementAnomaly::teleport:
            return RuleSignal::teleport_distance_exceeded;
        case MovementAnomaly::aim_snap:
            return RuleSignal::aim_snap_exceeded;
    }
    return std::nullopt;
}

void configure_tls(asio::ssl::context& context) {
    SSL_CTX* native_context = context.native_handle();
    if (SSL_CTX_set_min_proto_version(native_context, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(native_context, TLS1_3_VERSION) != 1) {
        throw std::runtime_error("could not restrict the TLS context to TLS 1.3");
    }
    constexpr auto required_options =
        SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION;
    if ((SSL_CTX_set_options(native_context, required_options) &
         required_options) != required_options) {
        throw std::runtime_error("could not apply required TLS context options");
    }
}

NetworkServiceConfig validate_config(
    NetworkServiceConfig config,
    double fixed_delta_seconds,
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<asio::ssl::context>& tls_context) {
    const auto tick_interval =
        std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::duration<double>(fixed_delta_seconds));
    if (!io_context || !tls_context ||
        config.maximum_connections == 0 ||
        config.authentication_timeout <= std::chrono::milliseconds::zero() ||
        config.frame_timeout <= std::chrono::milliseconds::zero() ||
        !std::isfinite(config.movement_rate_capacity) ||
        config.movement_rate_capacity < 1.0 ||
        !std::isfinite(config.movement_rate_per_second) ||
        config.movement_rate_per_second <= 0.0 ||
        !std::isfinite(config.ip_rate_capacity) ||
        config.ip_rate_capacity < 1.0 ||
        !std::isfinite(config.ip_rate_per_second) ||
        config.ip_rate_per_second <= 0.0 ||
        config.maximum_tracked_ip_rates == 0 ||
        config.maximum_tracked_ip_rates > kMaximumTrackedIpAddresses ||
        config.maximum_queued_intents_per_player == 0 ||
        config.maximum_queued_intents_per_player >
            kMaximumQueuedIntentsPerPlayer ||
        tick_interval <= SteadyClock::duration::zero() ||
        !config.on_error || !config.on_detection) {
        throw std::invalid_argument("network service configuration is invalid");
    }
    validate_movement_anomaly_config(config.movement_anomaly);
    return config;
}

std::shared_ptr<asio::ssl::context> configure_tls_context(
    std::shared_ptr<asio::ssl::context> context) {
    configure_tls(*context);
    return context;
}

void log_lifecycle(
    const NetworkServiceConfig& config,
    std::string_view event,
    const void* object) {
    static_cast<void>(object);
    write_json_log("info", "network_lifecycle", event);
    if (config.on_lifecycle) {
        config.on_lifecycle(event, object);
    }
}

}  // namespace

class NetworkService::Impl final
    : public std::enable_shared_from_this<NetworkService::Impl> {
public:
    class Connection final : public std::enable_shared_from_this<Connection> {
    public:
        Connection(
            std::shared_ptr<Impl> owner,
            Tcp::socket socket,
            std::string remote_ip);
        ~Connection();
        void start();
        void shutdown();
        void close();
        [[nodiscard]] std::optional<std::uint64_t> player_id() const noexcept;
        [[nodiscard]] SessionError session_status(
            SteadyClock::time_point now) const;

    private:
        void arm_timeout(std::chrono::milliseconds timeout);
        void cancel_timeout();
        void read_credential();
        void read_header();
        void read_payload();
        void begin_tls_shutdown();

        std::shared_ptr<Impl> owner_;
        asio::ssl::stream<Tcp::socket> stream_;
        std::string remote_ip_;
        asio::steady_timer timer_;
        std::array<std::uint8_t, kCredentialSize> credential_{};
        std::array<std::uint8_t, kMaximumFrameSize> frame_{};
        ProtocolHeader current_header_{};
        SequenceGuard sequence_guard_;
        TokenBucket limiter_;
        std::optional<MovementAnomalyDetector> movement_detector_;
        std::optional<std::uint64_t> player_id_;
        bool authenticated_{};
        bool hello_received_{};
        bool handshake_in_progress_{};
        bool shutdown_requested_{};
        bool shutdown_started_{};
        bool closed_{};
    };

    Impl(
        std::shared_ptr<asio::io_context> io_context,
        std::shared_ptr<asio::ssl::context> tls_context,
        SessionStore& sessions,
        Simulation& simulation,
        NetworkServiceConfig config)
        : config_(validate_config(
              std::move(config),
              simulation.fixed_delta_seconds(),
              io_context,
              tls_context)),
          io_context_(std::move(io_context)),
          tls_context_(configure_tls_context(std::move(tls_context))),
          sessions_(sessions),
          simulation_(simulation),
          detector_(config_.rule_detector),
          ip_rate_limiter_(
              config_.ip_rate_capacity,
              config_.ip_rate_per_second,
              config_.maximum_tracked_ip_rates),
          strand_(asio::make_strand(*io_context_)),
          acceptor_(strand_),
          tick_timer_(strand_),
          tick_interval_(std::chrono::duration_cast<SteadyClock::duration>(
              std::chrono::duration<double>(simulation.fixed_delta_seconds()))) {
        emit_lifecycle("TLS context acquired", tls_context_.get());
    }

    ~Impl() {
        emit_lifecycle("TLS context released", tls_context_.get());
    }

    void emit_lifecycle(std::string_view event, const void* object) const {
        log_lifecycle(config_, event, object);
    }

    void start() {
        if (started_ || stop_requested_.load(std::memory_order_acquire)) {
            throw std::logic_error("network service can only be started once");
        }

        asio::error_code error;
        acceptor_.open(config_.endpoint.protocol(), error);
        if (error) {
            throw asio::system_error(error, "open network service acceptor");
        }
        acceptor_.set_option(Tcp::acceptor::reuse_address(true), error);
        if (error) {
            throw asio::system_error(error, "configure network service acceptor");
        }
        acceptor_.bind(config_.endpoint, error);
        if (error) {
            throw asio::system_error(error, "bind network service acceptor");
        }
        acceptor_.listen(asio::socket_base::max_listen_connections, error);
        if (error) {
            throw asio::system_error(error, "listen on network service acceptor");
        }

        started_ = true;
        next_tick_ = SteadyClock::now() + tick_interval_;
        accept_next();
        tick();
    }

    void stop() {
        if (stop_requested_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        const auto self = shared_from_this();
        asio::dispatch(strand_, [self] {
            if (self->stopping_) {
                return;
            }
            self->stopping_ = true;

            asio::error_code ignored;
            self->acceptor_.cancel(ignored);
            self->acceptor_.close(ignored);
            self->tick_timer_.cancel();

            const auto connections = self->connections_;
            for (const auto& connection : connections) {
                connection->shutdown();
            }
            self->intents_by_player_.clear();
        });
    }

    [[nodiscard]] Tcp::endpoint local_endpoint() const {
        asio::error_code error;
        const auto endpoint = acceptor_.local_endpoint(error);
        if (error) {
            throw asio::system_error(error, "read network service endpoint");
        }
        return endpoint;
    }

private:
    friend class Connection;

    void accept_next() {
        if (stopping_) {
            return;
        }

        auto socket = std::make_shared<Tcp::socket>(strand_);
        const auto self = shared_from_this();
        acceptor_.async_accept(*socket, asio::bind_executor(
            strand_, [self, socket](const asio::error_code& error) {
                if (error) {
                    if (error != asio::error::operation_aborted &&
                        !self->stopping_) {
                        self->config_.on_error(error);
                        self->stop();
                    }
                    return;
                }

                if (self->connections_.size() >=
                    self->config_.maximum_connections) {
                    asio::error_code ignored;
                    socket->close(ignored);
                } else {
                    asio::error_code endpoint_error;
                    const auto remote = socket->remote_endpoint(endpoint_error);
                    std::string remote_ip;
                    if (!endpoint_error) {
                        remote_ip = remote.address().to_string();
                    }
                    if (endpoint_error) {
                        self->config_.on_error(endpoint_error);
                        asio::error_code ignored;
                        socket->close(ignored);
                    } else {
                        auto connection = std::make_shared<Connection>(
                            self, std::move(*socket), std::move(remote_ip));
                        self->connections_.insert(connection);
                        connection->start();
                    }
                }
                self->accept_next();
            }));
    }

    void tick() {
        if (stopping_) {
            return;
        }

        const auto self = shared_from_this();
        tick_timer_.expires_at(next_tick_);
        tick_timer_.async_wait(asio::bind_executor(
            strand_, [self](const asio::error_code& error) {
                if (error || self->stopping_) {
                    return;
                }

                std::vector<PlayerInput> inputs;
                std::vector<std::shared_ptr<Connection>> invalid_sessions;
                const auto now = SteadyClock::now();
                for (const auto& [player_id, weak_connection] :
                     self->active_players_) {
                    static_cast<void>(player_id);
                    if (const auto connection = weak_connection.lock()) {
                        const SessionError status = connection->session_status(now);
                        if (status == SessionError::crypto_failure) {
                            self->config_.on_error(service_error());
                            self->stop();
                            return;
                        }
                        if (status != SessionError::none) {
                            invalid_sessions.push_back(connection);
                        }
                    }
                }
                for (const auto& connection : invalid_sessions) {
                    connection->close();
                }

                inputs.reserve(self->intents_by_player_.size());
                for (auto& [player_id, intent_queue] :
                     self->intents_by_player_) {
                    inputs.push_back(PlayerInput{
                        player_id, intent_queue.advance_tick()});
                }
                if (self->simulation_.step(inputs) != SimulationError::none) {
                    self->config_.on_error(service_error());
                    self->stop();
                    return;
                }

                self->next_tick_ += self->tick_interval_;
                const auto tick_now = SteadyClock::now();
                if (self->next_tick_ <= tick_now) {
                    self->next_tick_ = tick_now + self->tick_interval_;
                }
                self->tick();
            }));
    }

    void register_player(
        const std::shared_ptr<Connection>& connection,
        std::uint64_t player_id,
        Vec2 movement) {
        if (active_players_.contains(player_id)) {
            connection->close();
            return;
        }
        active_players_.emplace(player_id, connection);
        intents_by_player_.try_emplace(
            player_id, config_.maximum_queued_intents_per_player);
        if (!enqueue_movement(player_id, movement)) {
            connection->close();
        }
    }

    [[nodiscard]] bool enqueue_movement(
        std::uint64_t player_id, Vec2 movement) {
        const auto queue = intents_by_player_.find(player_id);
        return queue != intents_by_player_.end() &&
               queue->second.enqueue(movement);
    }

    void report_rule_signal(std::uint64_t player_id, RuleSignal signal) {
        const auto result = detector_.observe(player_id, signal, simulation_.tick());
        if (result.error != RuleDetectorError::none) {
            config_.on_error(service_error());
            return;
        }
        if (config_.on_detection && result.finding) {
            config_.on_detection(*result.finding);
        }
    }

    void remove_connection(const std::shared_ptr<Connection>& connection) {
        connections_.erase(connection);
        const auto player_id = connection->player_id();
        if (!player_id) {
            return;
        }

        const auto active = active_players_.find(*player_id);
        if (active != active_players_.end() &&
            active->second.lock() == connection) {
            active_players_.erase(active);
            intents_by_player_.erase(*player_id);
        }
    }

    NetworkServiceConfig config_;
    std::shared_ptr<asio::io_context> io_context_;
    std::shared_ptr<asio::ssl::context> tls_context_;
    SessionStore& sessions_;
    Simulation& simulation_;
    RuleDetector detector_;
    IpRateLimiter ip_rate_limiter_;
    asio::strand<asio::io_context::executor_type> strand_;
    Tcp::acceptor acceptor_;
    asio::steady_timer tick_timer_;
    SteadyClock::duration tick_interval_;
    SteadyClock::time_point next_tick_{};
    std::set<std::shared_ptr<Connection>> connections_;
    std::map<std::uint64_t, std::weak_ptr<Connection>> active_players_;
    std::map<std::uint64_t, IntentQueue> intents_by_player_;
    bool started_{};
    bool stopping_{};
    std::atomic_bool stop_requested_{};
};

NetworkService::Impl::Connection::Connection(
    std::shared_ptr<Impl> owner, Tcp::socket socket, std::string remote_ip)
    : owner_(std::move(owner)),
      stream_(std::move(socket), *owner_->tls_context_),
      remote_ip_(std::move(remote_ip)),
      timer_(stream_.get_executor()),
      limiter_(
          owner_->config_.movement_rate_capacity,
          owner_->config_.movement_rate_per_second,
          SteadyClock::now()) {
    owner_->emit_lifecycle("session created", this);
}

NetworkService::Impl::Connection::~Connection() {
    owner_->emit_lifecycle("session destroyed", this);
}

void NetworkService::Impl::Connection::start() {
    arm_timeout(owner_->config_.authentication_timeout);
    handshake_in_progress_ = true;
    owner_->emit_lifecycle("handshake started", this);
    const auto self = shared_from_this();
    stream_.async_handshake(
        asio::ssl::stream_base::server,
        asio::bind_executor(owner_->strand_,
            [self](const asio::error_code& error) {
                self->handshake_in_progress_ = false;
                self->cancel_timeout();
                self->owner_->emit_lifecycle(
                    error ? "handshake finished with error"
                          : "handshake finished",
                    self.get());
                if (error) {
                    self->close();
                    return;
                }
                self->read_credential();
            }));
}

void NetworkService::Impl::Connection::shutdown() {
    if (closed_ || shutdown_requested_) {
        return;
    }
    shutdown_requested_ = true;
    cancel_timeout();
    asio::error_code ignored;
    stream_.lowest_layer().cancel(ignored);
}

void NetworkService::Impl::Connection::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    cancel_timeout();
    asio::error_code ignored;
    stream_.lowest_layer().cancel(ignored);
    stream_.lowest_layer().shutdown(Tcp::socket::shutdown_both, ignored);
    stream_.lowest_layer().close(ignored);
    OPENSSL_cleanse(credential_.data(), credential_.size());
    owner_->remove_connection(shared_from_this());
}

void NetworkService::Impl::Connection::begin_tls_shutdown() {
    if (closed_ || shutdown_started_ || handshake_in_progress_ ||
        !authenticated_) {
        if (!closed_ && !shutdown_started_) {
            close();
        }
        return;
    }
    shutdown_started_ = true;
    arm_timeout(std::chrono::milliseconds{250});
    owner_->emit_lifecycle("TLS shutdown started", this);
    const auto self = shared_from_this();
    stream_.async_shutdown(asio::bind_executor(
        owner_->strand_, [self](const asio::error_code& error) {
            self->cancel_timeout();
            self->owner_->emit_lifecycle(
                error ? "TLS shutdown finished with error"
                      : "TLS shutdown finished",
                self.get());
            self->close();
        }));
}

std::optional<std::uint64_t>
NetworkService::Impl::Connection::player_id() const noexcept {
    return player_id_;
}

SessionError NetworkService::Impl::Connection::session_status(
    SteadyClock::time_point now) const {
    if (!player_id_) {
        return SessionError::invalid_credential;
    }
    const auto validation = owner_->sessions_.validate(credential_, now);
    if (validation.error != SessionError::none) {
        return validation.error;
    }
    return validation.player_id == *player_id_
        ? SessionError::none
        : SessionError::invalid_credential;
}

void NetworkService::Impl::Connection::arm_timeout(
    std::chrono::milliseconds timeout) {
    timer_.expires_after(timeout);
    const auto self = shared_from_this();
    timer_.async_wait(asio::bind_executor(
        owner_->strand_, [self](const asio::error_code& error) {
            if (!error) {
                if (self->shutdown_started_) {
                    asio::error_code ignored;
                    self->stream_.lowest_layer().cancel(ignored);
                } else {
                    self->close();
                }
            }
        }));
}

void NetworkService::Impl::Connection::cancel_timeout() {
    timer_.cancel();
}

void NetworkService::Impl::Connection::read_credential() {
    arm_timeout(owner_->config_.authentication_timeout);
    const auto self = shared_from_this();
    asio::async_read(
        stream_, asio::buffer(credential_),
        asio::bind_executor(owner_->strand_,
            [self](const asio::error_code& error, std::size_t) {
                self->cancel_timeout();
                if (error) {
                    self->close();
                    return;
                }

                const auto validation = self->owner_->sessions_.validate(
                    self->credential_, SteadyClock::now());
                if (validation.error == SessionError::crypto_failure) {
                    self->owner_->config_.on_error(service_error());
                    self->close();
                    self->owner_->stop();
                    return;
                }
                const auto player =
                    self->owner_->simulation_.player(validation.player_id);
                if (validation.error != SessionError::none || !player) {
                    self->close();
                    return;
                }
                self->player_id_ = validation.player_id;
                self->movement_detector_.emplace(
                    self->owner_->config_.movement_anomaly,
                    player->position,
                    SteadyClock::now());
                self->authenticated_ = true;
                self->owner_->register_player(
                    self, validation.player_id, {});
                if (!self->closed_) {
                    self->read_header();
                }
            }));
}

void NetworkService::Impl::Connection::read_header() {
    if (closed_) {
        return;
    }
    arm_timeout(owner_->config_.frame_timeout);
    const auto self = shared_from_this();
    asio::async_read(
        stream_, asio::buffer(frame_.data(), kProtocolHeaderSize),
        asio::bind_executor(owner_->strand_,
            [self](const asio::error_code& error, std::size_t) {
                if (error) {
                    self->cancel_timeout();
                    if (self->shutdown_requested_ && self->authenticated_) {
                        self->begin_tls_shutdown();
                    } else {
                        self->close();
                    }
                    return;
                }

                const auto header = decode_protocol_header(
                    std::span<const std::uint8_t>(
                        self->frame_.data(), kProtocolHeaderSize));
                if (header.error != ProtocolError::none ||
                    (header.message_type == kHelloMessageType &&
                     self->hello_received_) ||
                    (header.message_type == kMoveMessageType &&
                     !self->hello_received_)) {
                    self->cancel_timeout();
                    self->close();
                    return;
                }
                self->current_header_ = header;
                self->read_payload();
            }));
}

void NetworkService::Impl::Connection::read_payload() {
    const auto self = shared_from_this();
    const std::size_t payload_size = current_header_.payload_size;
    asio::async_read(
        stream_,
        asio::buffer(frame_.data() + kProtocolHeaderSize, payload_size),
        asio::bind_executor(owner_->strand_,
            [self](const asio::error_code& error, std::size_t) {
                self->cancel_timeout();
                if (error) {
                    if (self->shutdown_requested_ &&
                        self->authenticated_) {
                        self->begin_tls_shutdown();
                    } else {
                        self->close();
                    }
                    return;
                }

                const std::size_t frame_size =
                    kProtocolHeaderSize + self->current_header_.payload_size;
                const std::span<const std::uint8_t> bytes(
                    self->frame_.data(), frame_size);
                if (self->current_header_.message_type == kHelloMessageType) {
                    if (decode_hello_frame(bytes).error != ProtocolError::none) {
                        self->close();
                        return;
                    }
                    self->hello_received_ = true;
                    self->read_header();
                    return;
                }

                const auto decoded = decode_move_frame(self->frame_);
                if (decoded.error != ProtocolError::none) {
                    const bool out_of_range =
                        decoded.error == ProtocolError::invalid_movement;
                    const auto player_id = self->player_id_;
                    self->close();
                    if (out_of_range && player_id) {
                        self->owner_->report_rule_signal(
                            *player_id, RuleSignal::movement_out_of_range);
                    }
                    return;
                }
                const auto session_error = self->owner_->sessions_.validate(
                    self->credential_, SteadyClock::now()).error;
                if (session_error == SessionError::crypto_failure) {
                    self->owner_->config_.on_error(service_error());
                    self->close();
                    self->owner_->stop();
                    return;
                }
                if (session_error != SessionError::none) {
                    self->close();
                    return;
                }
                const ProtocolError sequence_error =
                    self->sequence_guard_.accept(decoded.frame.sequence);
                if (sequence_error != ProtocolError::none) {
                    const auto player_id = self->player_id_;
                    self->close();
                    if (sequence_error == ProtocolError::replay_or_out_of_order &&
                        player_id) {
                        self->owner_->report_rule_signal(
                            *player_id, RuleSignal::replayed_sequence);
                    }
                    return;
                }
                const auto now = SteadyClock::now();
                if (!self->owner_->ip_rate_limiter_.consume(
                        self->remote_ip_, now)) {
                    const auto player_id = self->player_id_;
                    self->close();
                    if (player_id) {
                        self->owner_->report_rule_signal(
                            *player_id, RuleSignal::ip_rate_exceeded);
                    }
                    return;
                }
                if (!self->limiter_.consume(now)) {
                    const auto player_id = self->player_id_;
                    self->close();
                    if (player_id) {
                        self->owner_->report_rule_signal(
                            *player_id, RuleSignal::movement_rate_exceeded);
                    }
                    return;
                }

                if (!self->movement_detector_) {
                    const auto player_id = self->player_id_;
                    self->close();
                    if (player_id) {
                        self->owner_->config_.on_error(service_error());
                    }
                    return;
                }
                const MovementAnomaly anomaly =
                    self->movement_detector_->observe(decoded.frame, SteadyClock::now());
                if (anomaly != MovementAnomaly::none) {
                    const auto player_id = self->player_id_;
                    self->close();
                    const auto signal = movement_rule_signal(anomaly);
                    if (signal && player_id) {
                        self->owner_->report_rule_signal(*player_id, *signal);
                    } else {
                        self->owner_->config_.on_error(service_error());
                    }
                    return;
                }

                if (!self->owner_->enqueue_movement(
                        *self->player_id_,
                        Vec2{decoded.frame.x, decoded.frame.y})) {
                    const auto player_id = self->player_id_;
                    self->close();
                    if (player_id) {
                        self->owner_->report_rule_signal(
                            *player_id, RuleSignal::movement_intent_queue_full);
                    }
                    return;
                }
                self->read_header();
            }));
}

NetworkService::NetworkService(
    std::shared_ptr<asio::io_context> io_context,
    std::shared_ptr<asio::ssl::context> tls_context,
    SessionStore& sessions,
    Simulation& simulation,
    NetworkServiceConfig config)
    : impl_(std::make_shared<Impl>(
          std::move(io_context),
          std::move(tls_context),
          sessions,
          simulation,
          std::move(config))) {}

NetworkService::~NetworkService() {
    stop();
}

void NetworkService::start() {
    impl_->start();
}

void NetworkService::stop() {
    impl_->stop();
}

asio::ip::tcp::endpoint NetworkService::local_endpoint() const {
    return impl_->local_endpoint();
}

}  // namespace aegisguard
