#include "aegisguard/network_service.hpp"

#include "aegisguard/protocol.hpp"

#include <gtest/gtest.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace aegisguard {
namespace {

using namespace std::chrono_literals;

struct PkeyDeleter {
    void operator()(EVP_PKEY* key) const noexcept {
        EVP_PKEY_free(key);
    }
};

struct X509Deleter {
    void operator()(X509* certificate) const noexcept {
        X509_free(certificate);
    }
};

std::unique_ptr<EVP_PKEY, PkeyDeleter> make_test_key() {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048) <= 0) {
        throw std::runtime_error("could not initialize test certificate key");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_keygen(context.get(), &raw_key) <= 0) {
        throw std::runtime_error("could not generate test certificate key");
    }
    return std::unique_ptr<EVP_PKEY, PkeyDeleter>(raw_key);
}

std::unique_ptr<X509, X509Deleter> make_test_certificate(EVP_PKEY* key) {
    std::unique_ptr<X509, X509Deleter> certificate(X509_new());
    if (!certificate || X509_set_version(certificate.get(), 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -60) == nullptr ||
        X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 3600) == nullptr ||
        X509_set_pubkey(certificate.get(), key) != 1) {
        throw std::runtime_error("could not initialize test certificate");
    }

    X509_NAME* subject = X509_get_subject_name(certificate.get());
    constexpr unsigned char common_name[] = "localhost";
    if (X509_NAME_add_entry_by_txt(
            subject, "CN", MBSTRING_ASC, common_name, -1, -1, 0) != 1 ||
        X509_set_issuer_name(certificate.get(), subject) != 1 ||
        X509_sign(certificate.get(), key, EVP_sha256()) <= 0) {
        throw std::runtime_error("could not sign test certificate");
    }
    return certificate;
}

void install_test_certificate(asio::ssl::context& context) {
    auto key = make_test_key();
    auto certificate = make_test_certificate(key.get());
    SSL_CTX* native_context = context.native_handle();
    if (SSL_CTX_use_certificate(native_context, certificate.get()) != 1 ||
        SSL_CTX_use_PrivateKey(native_context, key.get()) != 1 ||
        SSL_CTX_check_private_key(native_context) != 1) {
        throw std::runtime_error("could not install test TLS certificate");
    }
}

std::array<std::uint8_t, kMaximumFrameSize> make_frame(
    std::uint64_t sequence, std::int16_t x, std::int16_t y,
    std::uint16_t angle = 0, std::int32_t position_x = 10000,
    std::int32_t position_y = 10000) {
    std::array<std::uint8_t, kMaximumFrameSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = kProtocolVersion;
    bytes[3] = kMoveMessageType;
    bytes[5] = static_cast<std::uint8_t>(kMovePayloadSize);
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned int>((7 - index) * 8);
        bytes[6 + index] = static_cast<std::uint8_t>(sequence >> shift);
    }

    const auto x_bits = static_cast<std::uint16_t>(x);
    const auto y_bits = static_cast<std::uint16_t>(y);
    bytes[14] = static_cast<std::uint8_t>(x_bits >> 8U);
    bytes[15] = static_cast<std::uint8_t>(x_bits);
    bytes[16] = static_cast<std::uint8_t>(y_bits >> 8U);
    bytes[17] = static_cast<std::uint8_t>(y_bits);
    bytes[18] = static_cast<std::uint8_t>(angle >> 8U);
    bytes[19] = static_cast<std::uint8_t>(angle);
    for (std::size_t index = 0; index < 4; ++index) {
        const auto shift = static_cast<unsigned int>((3 - index) * 8);
        bytes[20 + index] = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(position_x) >> shift);
        bytes[24 + index] = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(position_y) >> shift);
    }
    return bytes;
}

std::array<std::uint8_t, kHelloFrameSize> make_hello() {
    std::array<std::uint8_t, kHelloFrameSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = kProtocolVersion;
    bytes[3] = kHelloMessageType;
    bytes[5] = static_cast<std::uint8_t>(kHelloPayloadSize);
    bytes[15] = kProtocolVersion;
    return bytes;
}

std::array<std::uint8_t, kProtocolHeaderSize + kFirePayloadSize>
make_fire_frame(
    std::uint64_t sequence, std::uint16_t angle, std::uint64_t tick_reference) {
    std::array<std::uint8_t, kProtocolHeaderSize + kFirePayloadSize> bytes{};
    bytes[0] = 0x41;
    bytes[1] = 0x47;
    bytes[2] = kProtocolVersion;
    bytes[3] = kFireMessageType;
    bytes[5] = static_cast<std::uint8_t>(kFirePayloadSize);
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned int>((7 - index) * 8);
        bytes[6 + index] = static_cast<std::uint8_t>(sequence >> shift);
        bytes[kProtocolHeaderSize + 2 + index] =
            static_cast<std::uint8_t>(tick_reference >> shift);
    }
    bytes[kProtocolHeaderSize] = static_cast<std::uint8_t>(angle >> 8U);
    bytes[kProtocolHeaderSize + 1] = static_cast<std::uint8_t>(angle);
    return bytes;
}

std::array<std::uint8_t, kProtocolHeaderSize + kFirePayloadSize + 8>
make_claimed_fire_frame(std::uint64_t sequence) {
    auto valid = make_fire_frame(sequence, 0, 0);
    std::array<std::uint8_t, kProtocolHeaderSize + kFirePayloadSize + 8> bytes{};
    std::copy(valid.begin(), valid.end(), bytes.begin());
    const auto claimed_payload_size = kFirePayloadSize + 8;
    bytes[4] = static_cast<std::uint8_t>(claimed_payload_size >> 8U);
    bytes[5] = static_cast<std::uint8_t>(claimed_payload_size);
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(
                  kProtocolHeaderSize + kFirePayloadSize),
              bytes.end(), 0xff);
    return bytes;
}

class NetworkServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        install_test_certificate(*server_tls_);
        ASSERT_EQ(simulation_.add_player(7, Vec2{10.0, 10.0}),
                  SimulationError::none);
        ASSERT_EQ(simulation_.add_player(8, Vec2{15.0, 10.0}),
                  SimulationError::none);
        const auto issued = sessions_.issue(7, 60000ms, SessionStore::Clock::now());
        ASSERT_EQ(issued.error, SessionError::none);
        credential_ = issued.credential;

        NetworkServiceConfig config;
        config.maximum_connections = 1;
        config.authentication_timeout = 500ms;
        config.frame_timeout = 150ms;
        config.movement_rate_capacity = 2.0;
        config.movement_rate_per_second = 1.0;
        config.movement_anomaly.maximum_aim_rate_radians_per_second = 10.0;
        config.on_error = [this](std::error_code) {
            service_error_count_.fetch_add(1, std::memory_order_relaxed);
        };
        config.on_detection = [this](const RuleFinding& finding) {
            std::lock_guard lock(detection_mutex_);
            findings_.push_back(finding);
        };
        config.on_lifecycle = [this](
                                  std::string_view event, const void*) {
            {
                std::lock_guard lock(lifecycle_mutex_);
                lifecycle_events_.emplace_back(event);
            }
            lifecycle_condition_.notify_all();
        };
        customize_network_config(config);
        service_ = std::make_unique<NetworkService>(
            server_io_, server_tls_, sessions_, simulation_, std::move(config));
        service_->start();
        const auto io_context = server_io_;
        worker_ = std::thread([io_context] { io_context->run(); });
    }

    void TearDown() override {
        if (service_) {
            service_->stop();
        }
        if (worker_.joinable()) {
            // run() returns only after the service's canceled handlers drain.
            worker_.join();
        }
        service_.reset();
    }

    virtual void customize_network_config(NetworkServiceConfig&) {}

    asio::ssl::stream<asio::ip::tcp::socket> connect_client(
        asio::io_context& client_io,
        asio::ssl::context& client_tls) {
        asio::ssl::stream<asio::ip::tcp::socket> client(client_io, client_tls);
        client.lowest_layer().connect(service_->local_endpoint());
        client.handshake(asio::ssl::stream_base::client);
        return client;
    }

    void authenticate(
        asio::ssl::stream<asio::ip::tcp::socket>& client,
        const SessionCredential& credential) {
        asio::write(client, asio::buffer(credential));
        const auto hello = make_hello();
        asio::write(client, asio::buffer(hello));
    }

    std::optional<PlayerState> player_state(std::uint64_t player_id = 7) {
        auto promise = std::make_shared<std::promise<std::optional<PlayerState>>>();
        auto result = promise->get_future();
        asio::post(*server_io_, [this, promise, player_id] {
            promise->set_value(simulation_.player(player_id));
        });
        if (result.wait_for(1s) != std::future_status::ready) {
            return std::nullopt;
        }
        return result.get();
    }

    std::uint64_t simulation_tick() {
        auto promise = std::make_shared<std::promise<std::uint64_t>>();
        auto result = promise->get_future();
        asio::post(*server_io_, [this, promise] {
            promise->set_value(simulation_.tick());
        });
        if (result.wait_for(1s) != std::future_status::ready) {
            throw std::runtime_error("simulation tick query timed out");
        }
        return result.get();
    }

    bool wait_for_player_health(std::uint64_t player_id, std::uint32_t health) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto state = player_state(player_id);
            if (state && state->health == health) {
                return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    bool wait_for_player_position_change(double initial_x) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto state = player_state();
            if (state && state->position.x > initial_x) {
                return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    bool wait_for_player_velocity_x(double expected) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto state = player_state();
            if (state && state->velocity.x == expected) {
                return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    bool wait_for_player_velocity_y(double expected) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto state = player_state();
            if (state && state->velocity.y == expected) {
                return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    std::vector<RuleFinding> detection_findings() {
        std::lock_guard lock(detection_mutex_);
        return findings_;
    }

    bool wait_for_detection_count(std::size_t expected) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard lock(detection_mutex_);
                if (findings_.size() >= expected) {
                    return true;
                }
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    bool wait_for_lifecycle_event(std::string_view expected) {
        std::unique_lock lock(lifecycle_mutex_);
        return lifecycle_condition_.wait_for(lock, 1s, [this, expected] {
            return std::find(
                       lifecycle_events_.begin(),
                       lifecycle_events_.end(),
                       expected) != lifecycle_events_.end();
        });
    }

    bool wait_for_close(
        asio::ssl::stream<asio::ip::tcp::socket>& client,
        asio::io_context& client_io) {
        std::promise<asio::error_code> result;
        auto completed = result.get_future();
        bool timed_out = false;
        asio::steady_timer deadline(client_io);
        deadline.expires_after(2s);
        deadline.async_wait([&](const asio::error_code& error) {
            if (!error) {
                timed_out = true;
                asio::error_code ignored;
                client.lowest_layer().cancel(ignored);
            }
        });
        std::array<std::uint8_t, 1> byte{};
        client.async_read_some(asio::buffer(byte),
            [&](const asio::error_code& error, std::size_t) {
                deadline.cancel();
                result.set_value(error);
            });
        client_io.run();
        return !timed_out && completed.wait_for(0s) == std::future_status::ready &&
               static_cast<bool>(completed.get());
    }

    std::shared_ptr<asio::io_context> server_io_{
        std::make_shared<asio::io_context>()};
    std::shared_ptr<asio::ssl::context> server_tls_{
        std::make_shared<asio::ssl::context>(asio::ssl::context::tls_server)};
    SessionStore sessions_;
    Simulation simulation_;
    SessionCredential credential_{};
    std::atomic_size_t service_error_count_{};
    std::mutex detection_mutex_;
    std::vector<RuleFinding> findings_;
    std::mutex lifecycle_mutex_;
    std::condition_variable lifecycle_condition_;
    std::vector<std::string> lifecycle_events_;
    std::unique_ptr<NetworkService> service_;
    std::thread worker_;
};

class NetworkServiceIpRateTest : public NetworkServiceTest {
protected:
    void customize_network_config(NetworkServiceConfig& config) override {
        config.ip_rate_capacity = 2.0;
        config.ip_rate_per_second = 0.001;
    }
};

TEST(NetworkServiceConfigTest, RequiresDetectionHandlerToAvoidDroppingFindings) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto io_context = std::make_shared<asio::io_context>();
        auto tls_context = std::make_shared<asio::ssl::context>(
            asio::ssl::context::tls_server);
        SessionStore sessions;
        Simulation simulation;
        NetworkServiceConfig config;
        config.on_error = [](std::error_code) {};

        EXPECT_THROW(
            NetworkService service(
                io_context, tls_context, sessions, simulation, std::move(config)),
            std::invalid_argument);
    }
}

TEST(NetworkServiceConfigTest, RejectsInvalidServiceLimitsAndDependencies) {
    auto io_context = std::make_shared<asio::io_context>();
    auto tls_context = std::make_shared<asio::ssl::context>(
        asio::ssl::context::tls_server);
    SessionStore sessions;
    Simulation simulation;

    const auto valid_config = [] {
        NetworkServiceConfig config;
        config.on_error = [](std::error_code) {};
        config.on_detection = [](const RuleFinding&) {};
        return config;
    };
    const auto reject = [&](NetworkServiceConfig config) {
        EXPECT_THROW(
            NetworkService service(
                io_context, tls_context, sessions, simulation, config),
            std::invalid_argument);
    };
    const auto reject_config = [&](std::string_view label, auto mutate) {
        SCOPED_TRACE(label);
        auto config = valid_config();
        mutate(config);
        reject(config);
    };

    reject_config("maximum connections", [](NetworkServiceConfig& config) {
        config.maximum_connections = 0;
    });
    reject_config("authentication timeout", [](NetworkServiceConfig& config) {
        config.authentication_timeout = 0ms;
    });
    reject_config("frame timeout", [](NetworkServiceConfig& config) {
        config.frame_timeout = 0ms;
    });
    reject_config("NaN movement rate capacity", [](NetworkServiceConfig& config) {
        config.movement_rate_capacity =
            std::numeric_limits<double>::quiet_NaN();
    });
    reject_config("movement rate capacity", [](NetworkServiceConfig& config) {
        config.movement_rate_capacity = 0.5;
    });
    reject_config("movement rate", [](NetworkServiceConfig& config) {
        config.movement_rate_per_second = 0.0;
    });
    reject_config("IP rate capacity", [](NetworkServiceConfig& config) {
        config.ip_rate_capacity = 0.5;
    });
    reject_config("IP rate", [](NetworkServiceConfig& config) {
        config.ip_rate_per_second = 0.0;
    });
    reject_config("tracked IP lower bound", [](NetworkServiceConfig& config) {
        config.maximum_tracked_ip_rates = 0;
    });
    reject_config("tracked IP upper bound", [](NetworkServiceConfig& config) {
        config.maximum_tracked_ip_rates =
            std::numeric_limits<std::size_t>::max();
    });
    reject_config("queued intent lower bound", [](NetworkServiceConfig& config) {
        config.maximum_queued_intents_per_player = 0;
    });
    reject_config("queued intent upper bound", [](NetworkServiceConfig& config) {
        config.maximum_queued_intents_per_player = 1025;
    });
    reject_config("missing error handler", [](NetworkServiceConfig& config) {
        config.on_error = {};
    });

    auto missing_detection_handler = valid_config();
    missing_detection_handler.on_detection = {};
    reject(missing_detection_handler);

    const auto config = valid_config();
    EXPECT_THROW(
        NetworkService service(
            std::shared_ptr<asio::io_context>{}, tls_context, sessions,
            simulation, config),
        std::invalid_argument);
    EXPECT_THROW(
        NetworkService service(
            io_context, std::shared_ptr<asio::ssl::context>{}, sessions,
            simulation, config),
        std::invalid_argument);

    SimulationConfig tiny_tick_config;
    tiny_tick_config.fixed_delta_seconds = 1e-20;
    Simulation tiny_tick_simulation(tiny_tick_config);
    EXPECT_THROW(
        NetworkService service(
            io_context, tls_context, sessions, tiny_tick_simulation, config),
        std::invalid_argument);
}

TEST_F(NetworkServiceTest, AuthenticatedMovementUsesServerFixedTicks) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_player_position_change(10.0));
    EXPECT_EQ(service_error_count_.load(std::memory_order_relaxed), 0U);
}

TEST_F(NetworkServiceTest, RejectsUnknownCredentials) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    const SessionCredential unknown{};
    authenticate(client, unknown);

    EXPECT_TRUE(wait_for_close(client, client_io));
}

TEST_F(NetworkServiceTest, ClosesPartialFramesAfterTheReadDeadline) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto frame = make_frame(1, 1000, 0);
    asio::write(
        client, asio::buffer(frame.data(), kProtocolHeaderSize));

    EXPECT_TRUE(wait_for_close(client, client_io));
}

TEST_F(NetworkServiceTest, RejectsOversizedFrameOnLoopback) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    std::array<std::uint8_t, kProtocolHeaderSize> oversized_header{
        0x41, 0x47, kProtocolVersion, kMoveMessageType,
        0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 1};
    asio::write(client, asio::buffer(oversized_header));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->position.x, 10.0);
}

TEST_F(NetworkServiceTest, RejectsOldProtocolVersion) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    asio::write(client, asio::buffer(credential_));

    auto old_frame = make_frame(1, 0, 0);
    old_frame[2] = 1;
    asio::write(client, asio::buffer(old_frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
}

TEST_F(NetworkServiceTest, RejectsMovementBeforeHello) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    asio::write(client, asio::buffer(credential_));

    const auto frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->position.x, 10.0);
}

TEST_F(NetworkServiceTest, ClosesUnauthenticatedConnectionsAfterTheDeadline) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);

    EXPECT_TRUE(wait_for_close(client, client_io));
}

TEST_F(NetworkServiceTest, RejectsMalformedFrameWithoutChangingPlayerState) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    auto frame = make_frame(1, 1000, 0);
    frame[0] = 0;
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->position.x, 10.0);
}

TEST_F(NetworkServiceTest, ReportsOutOfRangeMovementAsTestOnlyEvidence) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto frame = make_frame(1, 1001, 0);
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    const auto findings = detection_findings();
    ASSERT_EQ(findings.size(), 1U);
    EXPECT_EQ(findings[0].signal, RuleSignal::movement_out_of_range);
    EXPECT_EQ(findings[0].observations_in_window, 1U);
    EXPECT_EQ(findings[0].recommended_action, RecommendedAction::observe);
    EXPECT_EQ(findings[0].applied_action, RecommendedAction::observe);
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->position.x, 10.0);
}

TEST_F(NetworkServiceTest, ForgedPositionRejected) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto frame = make_frame(1, 1000, 0, 0, 1000000, 10000);
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    EXPECT_EQ(detection_findings()[0].signal,
              RuleSignal::teleport_distance_exceeded);
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->position.x, 10.0);
}

TEST_F(NetworkServiceTest, SpeedCheatRejected) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto baseline = make_frame(1, 0, 0);
    const auto excessive_speed = make_frame(2, 1000, 0, 0, 19000, 10000);
    asio::write(client, asio::buffer(baseline));
    asio::write(client, asio::buffer(excessive_speed));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    EXPECT_EQ(detection_findings()[0].signal, RuleSignal::speed_exceeded);
}

TEST_F(NetworkServiceTest, TeleportCheatRejected) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto teleport = make_frame(1, 1000, 0, 0, 21001, 10000);
    asio::write(client, asio::buffer(teleport));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    EXPECT_EQ(detection_findings()[0].signal,
              RuleSignal::teleport_distance_exceeded);
}

TEST_F(NetworkServiceTest, AimSnapCheatRejected) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto baseline = make_frame(1, 0, 0, 0);
    const auto snap = make_frame(2, 0, 0, 32768);
    asio::write(client, asio::buffer(baseline));
    asio::write(client, asio::buffer(snap));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    EXPECT_EQ(detection_findings()[0].signal, RuleSignal::aim_snap_exceeded);
}

TEST_F(NetworkServiceTest, AuthenticatedFireUsesServerValidatedHitAndHealth) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);
    const auto target_before = player_state(8);
    ASSERT_TRUE(target_before);

    const auto frame = make_fire_frame(1, 0, simulation_tick());
    asio::write(client, asio::buffer(frame));

    ASSERT_TRUE(wait_for_player_health(8, 75));
    EXPECT_EQ(player_state(7)->health, 100U);
    EXPECT_DOUBLE_EQ(player_state(8)->position.x, target_before->position.x);
    EXPECT_DOUBLE_EQ(player_state(8)->position.y, target_before->position.y);
}

TEST_F(NetworkServiceTest, RejectsForgedHealthAndHitClaimsWithoutChangingState) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);
    const auto shooter_before = player_state(7);
    const auto target_before = player_state(8);
    ASSERT_TRUE(shooter_before);
    ASSERT_TRUE(target_before);

    const auto forged_frame = make_claimed_fire_frame(1);
    asio::write(client, asio::buffer(forged_frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto shooter_after = player_state(7);
    const auto target_after = player_state(8);
    ASSERT_TRUE(shooter_after);
    ASSERT_TRUE(target_after);
    EXPECT_EQ(shooter_after->health, shooter_before->health);
    EXPECT_EQ(target_after->health, target_before->health);
    EXPECT_DOUBLE_EQ(target_after->position.x, target_before->position.x);
    EXPECT_DOUBLE_EQ(target_after->position.y, target_before->position.y);
}

TEST_F(NetworkServiceTest, ClosesConnectionsWhenASequenceIsReplayed) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto frame = make_frame(2, 500, 0);
    asio::write(client, asio::buffer(frame));
    asio::write(client, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    const auto findings = detection_findings();
    ASSERT_EQ(findings.size(), 1U);
    EXPECT_EQ(findings[0].signal, RuleSignal::replayed_sequence);
}

TEST_F(NetworkServiceTest, TracksRepeatedReplaySignalsAcrossConnections) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);

    for (std::size_t attempt = 0; attempt < 2; ++attempt) {
        if (attempt != 0) {
            client_io.restart();
        }
        auto client = connect_client(client_io, client_tls);
        authenticate(client, credential_);

        const auto frame = make_frame(1, 0, 0);
        asio::write(client, asio::buffer(frame));
        asio::write(client, asio::buffer(frame));
        ASSERT_TRUE(wait_for_close(client, client_io));
        ASSERT_TRUE(wait_for_detection_count(attempt + 1));
    }

    const auto findings = detection_findings();
    ASSERT_EQ(findings.size(), 2U);
    EXPECT_EQ(findings[0].observations_in_window, 1U);
    EXPECT_EQ(findings[1].observations_in_window, 2U);
    EXPECT_EQ(findings[1].recommended_action, RecommendedAction::review);
    EXPECT_EQ(findings[1].applied_action, RecommendedAction::observe);
}

TEST_F(NetworkServiceTest, RechecksCredentialRevocationForEveryFrame) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto moving_frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(moving_frame));
    ASSERT_TRUE(wait_for_player_position_change(10.0));

    const auto stop_frame = make_frame(2, 0, 0);
    asio::write(client, asio::buffer(stop_frame));
    ASSERT_TRUE(wait_for_player_velocity_x(0.0));
    const auto state = player_state();
    ASSERT_TRUE(state);
    const double position_before_revocation = state->position.x;

    ASSERT_EQ(sessions_.revoke(credential_), SessionError::none);
    const auto revoked_frame = make_frame(3, -1000, 0);
    asio::write(client, asio::buffer(revoked_frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto final_state = player_state();
    ASSERT_TRUE(final_state);
    EXPECT_DOUBLE_EQ(final_state->position.x, position_before_revocation);
    EXPECT_DOUBLE_EQ(final_state->velocity.x, 0.0);
}

TEST_F(NetworkServiceTest, StopsMovementWhenAnIdleSessionExpires) {
    const auto issued = sessions_.issue(
        7, SessionStore::Duration{1000}, SessionStore::Clock::now());
    ASSERT_EQ(issued.error, SessionError::none);

    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, issued.credential);

    const auto frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(frame));
    ASSERT_TRUE(wait_for_player_position_change(10.0));
    ASSERT_TRUE(wait_for_player_velocity_x(0.0));

    EXPECT_TRUE(wait_for_close(client, client_io));
    const auto state = player_state();
    ASSERT_TRUE(state);
    EXPECT_DOUBLE_EQ(state->velocity.x, 0.0);
}

TEST_F(NetworkServiceTest, RejectsTlsTwelveClients) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    ASSERT_EQ(SSL_CTX_set_min_proto_version(
                  client_tls.native_handle(), TLS1_2_VERSION),
              1);
    ASSERT_EQ(SSL_CTX_set_max_proto_version(
                  client_tls.native_handle(), TLS1_2_VERSION),
              1);
    asio::ssl::stream<asio::ip::tcp::socket> client(client_io, client_tls);
    client.lowest_layer().connect(service_->local_endpoint());

    EXPECT_THROW(
        client.handshake(asio::ssl::stream_base::client),
        asio::system_error);
}

TEST_F(NetworkServiceTest, RejectsConnectionsAboveTheConfiguredLimit) {
    asio::io_context first_io;
    asio::ssl::context first_tls(asio::ssl::context::tls_client);
    first_tls.set_verify_mode(asio::ssl::verify_none);
    auto first = connect_client(first_io, first_tls);

    asio::io_context second_io;
    asio::ssl::context second_tls(asio::ssl::context::tls_client);
    second_tls.set_verify_mode(asio::ssl::verify_none);
    asio::ssl::stream<asio::ip::tcp::socket> second(second_io, second_tls);
    second.lowest_layer().connect(service_->local_endpoint());

    EXPECT_THROW(
        second.handshake(asio::ssl::stream_base::client),
        asio::system_error);
}

TEST_F(NetworkServiceTest, FloodRateLimitRejected) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);

    const auto first_frame = make_frame(1, 1000, 0);
    const auto second_frame = make_frame(2, -1000, 0);
    const auto third_frame = make_frame(3, 1000, 0);
    asio::write(client, asio::buffer(first_frame));
    asio::write(client, asio::buffer(second_frame));
    asio::write(client, asio::buffer(third_frame));

    EXPECT_TRUE(wait_for_close(client, client_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    const auto findings = detection_findings();
    ASSERT_EQ(findings.size(), 1U);
    EXPECT_EQ(findings[0].signal, RuleSignal::movement_rate_exceeded);
}

TEST_F(NetworkServiceIpRateTest, PerIpRateLimitSurvivesReconnect) {
    asio::io_context first_io;
    asio::ssl::context first_tls(asio::ssl::context::tls_client);
    first_tls.set_verify_mode(asio::ssl::verify_none);
    auto first = connect_client(first_io, first_tls);
    authenticate(first, credential_);

    const auto move_x = make_frame(1, 1000, 0);
    const auto move_y = make_frame(2, 0, 1000);
    asio::write(first, asio::buffer(move_x));
    ASSERT_TRUE(wait_for_player_velocity_x(5.0));
    asio::write(first, asio::buffer(move_y));
    ASSERT_TRUE(wait_for_player_velocity_y(5.0));

    asio::error_code close_error;
    first.lowest_layer().close(close_error);
    ASSERT_TRUE(wait_for_lifecycle_event("session destroyed"));

    asio::io_context second_io;
    asio::ssl::context second_tls(asio::ssl::context::tls_client);
    second_tls.set_verify_mode(asio::ssl::verify_none);
    auto second = connect_client(second_io, second_tls);
    authenticate(second, credential_);
    const auto frame = make_frame(1, 0, 0);
    asio::write(second, asio::buffer(frame));

    EXPECT_TRUE(wait_for_close(second, second_io));
    ASSERT_TRUE(wait_for_detection_count(1));
    EXPECT_EQ(detection_findings()[0].signal, RuleSignal::ip_rate_exceeded);
}

TEST_F(NetworkServiceTest, StopsAnAuthenticatedConnectionWithTlsShutdown) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);
    const auto frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(frame));
    ASSERT_TRUE(wait_for_player_position_change(10.0));

    service_->stop();

    EXPECT_TRUE(wait_for_close(client, client_io));
    asio::error_code shutdown_error;
    client.shutdown(shutdown_error);
    EXPECT_FALSE(shutdown_error);
}

TEST_F(NetworkServiceTest, StopsDuringHandshakeWithoutStartingTlsShutdown) {
    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    asio::ssl::stream<asio::ip::tcp::socket> client(client_io, client_tls);
    client.lowest_layer().connect(service_->local_endpoint());
    ASSERT_TRUE(wait_for_lifecycle_event("handshake started"));

    service_->stop();

    EXPECT_TRUE(wait_for_lifecycle_event("handshake finished with error"));
    EXPECT_TRUE(wait_for_lifecycle_event("session destroyed"));
    {
        std::lock_guard lock(lifecycle_mutex_);
        EXPECT_EQ(
            std::find(
                lifecycle_events_.begin(),
                lifecycle_events_.end(),
                "TLS shutdown started"),
            lifecycle_events_.end());
    }
}

TEST_F(NetworkServiceTest, RetainsContextsUntilShutdownHandlersHaveDrained) {
    const std::weak_ptr<asio::io_context> io_context_lifetime = server_io_;
    const std::weak_ptr<asio::ssl::context> tls_context_lifetime = server_tls_;

    asio::io_context client_io;
    asio::ssl::context client_tls(asio::ssl::context::tls_client);
    client_tls.set_verify_mode(asio::ssl::verify_none);
    auto client = connect_client(client_io, client_tls);
    authenticate(client, credential_);
    const auto frame = make_frame(1, 1000, 0);
    asio::write(client, asio::buffer(frame));
    ASSERT_TRUE(wait_for_player_position_change(10.0));

    server_io_.reset();
    server_tls_.reset();
    EXPECT_FALSE(io_context_lifetime.expired());
    EXPECT_FALSE(tls_context_lifetime.expired());

    service_->stop();
    EXPECT_TRUE(wait_for_close(client, client_io));
    asio::error_code shutdown_error;
    client.shutdown(shutdown_error);
    if (worker_.joinable()) {
        worker_.join();
    }
    service_.reset();

    EXPECT_TRUE(io_context_lifetime.expired());
    EXPECT_TRUE(tls_context_lifetime.expired());
}

}  // namespace
}  // namespace aegisguard
