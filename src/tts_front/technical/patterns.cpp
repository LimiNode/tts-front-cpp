#include "tts_front/technical.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace tts_front::detail::technical {

const Patterns& patterns() {
    static const Patterns value;
    return value;
}

bool has_known_identifier_prefix(std::string_view value) {
    constexpr std::array<std::string_view, 5> prefixes = {"RTX", "CUDA", "GPU", "API", "C++"};
    for (const auto prefix : prefixes) {
        if (value.rfind(prefix, 0) == 0)
            return true;
    }
    return false;
}

std::vector<SourceRange> scan_pronunciation_candidates(std::string_view value) {
    const auto& source = patterns();
    std::vector<SourceRange> candidates;
    const auto append_exact = [&](const std::regex& pattern, std::string_view expected) {
        using Iterator = std::regex_iterator<std::string_view::const_iterator>;
        for (Iterator it(value.begin(), value.end(), pattern), end; it != end; ++it) {
            const auto offset = static_cast<std::size_t>(it->position());
            const auto length = static_cast<std::size_t>(it->length());
            if (value.substr(offset, length) != expected)
                continue;
            const auto before = offset == 0 ? '\0' : value[offset - 1];
            const auto after = offset + length == value.size() ? '\0' : value[offset + length];
            const auto is_word_byte = [](char byte) {
                return std::isalnum(static_cast<unsigned char>(byte)) != 0 || byte == '_';
            };
            if (is_word_byte(before) || is_word_byte(after))
                continue;
            if (expected == "RTX 4090" && offset + length < value.size()) {
                auto suffix = offset + length;
                while (suffix < value.size() &&
                       std::isspace(static_cast<unsigned char>(value[suffix])) != 0)
                    ++suffix;
                if (value.substr(suffix, 2) == "Ti" &&
                    (suffix + 2 == value.size() || !is_word_byte(value[suffix + 2])))
                    continue;
            }
            candidates.push_back({offset, length});
        }
    };
    append_exact(source.technical_gpu, "RTX 4090");
    append_exact(source.technical_cpp, "C++");
    append_exact(source.technical_http, "HTTP/2");
    std::sort(candidates.begin(), candidates.end(), [](const SourceRange& left,
                                                       const SourceRange& right) {
        return left.offset < right.offset ||
               (left.offset == right.offset && left.length > right.length);
    });
    return candidates;
}

} // namespace tts_front::detail::technical
