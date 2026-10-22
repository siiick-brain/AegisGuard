#include "aegisguard/network_service.hpp"

#include "aegisguard/protocol.hpp"

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
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

}  // namespace

class NetworkService::Impl final
    : public std::enable_shared_from_this<NetworkService::Impl> {
public:
    class Connection final : public std::enable_shared_from_this<Connection> {
    public:
        Connection(std::shared_ptr<Impl> owner, Tcp::socket socket);
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
        asio::steady_timer timer_;
        std::array<std::uint8_t, kCredentialSize> credential_{};
        std::array<std::uint8_t, kMaximumFrameSize> frame_{};
        SequenceGuard sequence_guard_;
        TokenBucket limiter_;
        std::optional<std::uint64_t> player_id_;
        bool authenticated_{};
        bool shutdown_requested_{};
        bool shutdown_started_{};
        bool closed_{};
    };

    Impl(
        asio::io_context& io_context,
        asio::ssl::context& tls_context,
        SessionStore& sessions,
        Simulation& simulation,
        NetworkServiceConfig config)
        : strand_(asio::make_strand(io_context)),
          tls_context_(tls_context),
          sessions_(sessions),
          simulation_(simulation),
          config_(std::move(config)),
          acceptor_(strand_),
          tick_timer_(strand_),
          tick_interval_(std::chrono::duration_cast<SteadyClock::duration>(
              std::chrono::duration<double>(simulation.fixed_delta_seconds()))) {
        if (config_.maximum_connections == 0 ||
            config_.authentication_timeout <= std::chrono::milliseconds::zero() ||
            config_.frame_timeout <= std::chrono::milliseconds::zero() ||
            !std::isfinite(config_.movement_rate_capacity) ||
            config_.movement_rate_capacity < 1.0 ||
            !std::isfinite(config_.movement_rate_per_second) ||
            config_.movement_rate_per_second <= 0.0 ||
            tick_interval_ <= SteadyClock::duration::zero() ||
            !config_.on_error) {
            throw std::invalid_argument("network service configuration is invalid");
        }
        configure_tls(tls_context_);
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
            self->movement_by_player_.clear();
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
                    auto connection = std::make_shared<Connection>(
                        self, std::move(*socket));
                    self->connections_.insert(connection);
                    connection->start();
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

                inputs.reserve(self->movement_by_player_.size());
                for (const auto& [player_id, movement] :
                     self->movement_by_player_) {
                    inputs.push_back(PlayerInput{player_id, movement});
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
        movement_by_player_.insert_or_assign(player_id, movement);
    }

    void update_movement(std::uint64_t player_id, Vec2 movement) {
        movement_by_player_.insert_or_assign(player_id, movement);
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
            movement_by_player_.erase(*player_id);
        }
    }

    asio::strand<asio::io_context::executor_type> strand_;
    asio::ssl::context& tls_context_;
    SessionStore& sessions_;
    Simulation& simulation_;
    NetworkServiceConfig config_;
    Tcp::acceptor acceptor_;
    asio::steady_timer tick_timer_;
    SteadyClock::duration tick_interval_;
    SteadyClock::time_point next_tick_{};
    std::set<std::shared_ptr<Connection>> connections_;
    std::map<std::uint64_t, std::weak_ptr<Connection>> active_players_;
    std::map<std::uint64_t, Vec2> movement_by_player_;
    bool started_{};
    bool stopping_{};
    std::atomic_bool stop_requested_{};
};

NetworkService::Impl::Connection::Connection(
    std::shared_ptr<Impl> owner, Tcp::socket socket)
    : owner_(std::move(owner)),
      stream_(std::move(socket), owner_->tls_context_),
      timer_(stream_.get_executor()),
      limiter_(
          owner_->config_.movement_rate_capacity,
          owner_->config_.movement_rate_per_second,
          SteadyClock::now()) {}

void NetworkService::Impl::Connection::start() {
    arm_timeout(owner_->config_.authentication_timeout);
    const auto self = shared_from_this();
    stream_.async_handshake(
        asio::ssl::stream_base::server,
        asio::bind_executor(owner_->strand_,
            [self](const asio::error_code& error) {
                self->cancel_timeout();
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
    if (!authenticated_) {
        close();
        return;
    }

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
    if (closed_ || shutdown_started_) {
        return;
    }
    shutdown_started_ = true;
    arm_timeout(std::chrono::milliseconds{250});
    const auto self = shared_from_this();
    stream_.async_shutdown(asio::bind_executor(
        owner_->strand_, [self](const asio::error_code&) {
            self->cancel_timeout();
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
                self->close();
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
                if (validation.error != SessionError::none ||
                    !self->owner_->simulation_.player(validation.player_id)) {
                    self->close();
                    return;
                }
                self->player_id_ = validation.player_id;
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

                const auto result = decode_move_frame(
                    std::span<const std::uint8_t>(
                        self->frame_.data(), kProtocolHeaderSize));
                if (result.error != ProtocolError::incomplete_frame) {
                    self->cancel_timeout();
                    self->close();
                    return;
                }
                self->read_payload();
            }));
}

void NetworkService::Impl::Connection::read_payload() {
    const auto self = shared_from_this();
    asio::async_read(
        stream_,
        asio::buffer(frame_.data() + kProtocolHeaderSize, kMovePayloadSize),
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

                const auto decoded = decode_move_frame(self->frame_);
                if (decoded.error != ProtocolError::none) {
                    self->close();
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
                if (session_error != SessionError::none ||
                    self->sequence_guard_.accept(decoded.frame.sequence) !=
                        ProtocolError::none ||
                    !self->limiter_.consume(SteadyClock::now())) {
                    self->close();
                    return;
                }

                self->owner_->update_movement(
                    *self->player_id_,
                    Vec2{decoded.frame.x, decoded.frame.y});
                self->read_header();
            }));
}

NetworkService::NetworkService(
    asio::io_context& io_context,
    asio::ssl::context& tls_context,
    SessionStore& sessions,
    Simulation& simulation,
    NetworkServiceConfig config)
    : impl_(std::make_shared<Impl>(
          io_context, tls_context, sessions, simulation, std::move(config))) {}

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
