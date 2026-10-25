#include "aegisguard/json_log.hpp"

#include <string>

int main() {
    for (int index = 0; index < 100; ++index) {
        aegisguard::write_json_log(
            "info", "json_log_probe", "record " + std::to_string(index));
    }
}
