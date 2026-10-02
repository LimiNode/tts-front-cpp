#include "utf8.hpp"

#include <stdexcept>
#include <string>

int main() {
    std::string output;
    try {
        silero_native::append_utf8(output, 0xd800);
    } catch (const std::runtime_error&) {
        return 0;
    }
    return 1;
}
