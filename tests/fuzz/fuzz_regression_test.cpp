#include "aegisguard/protocol.hpp"
#include "aegisguard/server_config.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

TEST(FuzzRegressionTest, ReplaysCheckedInParserInputs) {
    const std::filesystem::path corpus =
        std::filesystem::path(AEGISGUARD_FUZZ_REGRESSION_DIR);
    ASSERT_TRUE(std::filesystem::exists(corpus)) << corpus.string();

    std::size_t file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ifstream input(entry.path(), std::ios::binary);
        ASSERT_TRUE(input) << entry.path().string();
        const std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        const auto span = std::span<const std::uint8_t>(bytes);
        static_cast<void>(aegisguard::decode_protocol_header(span));
        static_cast<void>(aegisguard::decode_move_frame(span));
        static_cast<void>(aegisguard::decode_hello_frame(span));
        try {
            static_cast<void>(aegisguard::parse_server_config(
                std::string_view(
                    reinterpret_cast<const char*>(bytes.data()), bytes.size())));
        } catch (const std::exception&) {
        }
        ++file_count;
    }
    EXPECT_GE(file_count, 4U);
}

}  // namespace
