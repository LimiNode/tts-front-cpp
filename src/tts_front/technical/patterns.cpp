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

std::vector<SourceRange> scan_pattern_ranges(std::string_view value, const std::regex& pattern) {
    std::vector<SourceRange> ranges;
    const bool utf8_email = &pattern == &patterns().technical_utf8_email;
    if (!utf8_email && &pattern != &patterns().technical_email) {
        using Iterator = std::regex_iterator<std::string_view::const_iterator>;
        for (Iterator it(value.begin(), value.end(), pattern), end; it != end; ++it)
            ranges.push_back(
                {static_cast<std::size_t>(it->position()), static_cast<std::size_t>(it->length())});
        return ranges;
    }

    const auto letter = [](unsigned char byte) {
        return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
    };
    const auto digit = [](unsigned char byte) { return byte >= '0' && byte <= '9'; };
    const auto word = [&](unsigned char byte) {
        return letter(byte) || digit(byte) || byte == '_';
    };
    const auto local = [&](unsigned char byte) {
        return word(byte) || byte == '.' || byte == '%' || byte == '+' || byte == '-' ||
               (utf8_email && (byte == ',' || byte >= 0x80));
    };
    // Preserve both catalog byte grammars, including the UTF-8 rule's ASCII
    // word boundary, without recursive backtracking over long local parts.
    for (std::size_t begin = 0; begin < value.size();) {
        const bool starts = utf8_email
                                ? letter(value[begin]) && (begin == 0 || !word(value[begin - 1]))
                                : local(value[begin]);
        if (!starts) {
            ++begin;
            continue;
        }
        auto at = begin + 1;
        while (at < value.size() && local(value[at]))
            ++at;
        if (at == value.size() || value[at] != '@') {
            begin = at;
            continue;
        }
        auto cursor = at + 1;
        std::size_t finish = 0;
        while (cursor < value.size()) {
            const auto byte = value[cursor];
            if (byte == '.') {
                auto letters_end = cursor + 1;
                while (letters_end < value.size() && letter(value[letters_end]))
                    ++letters_end;
                if (cursor > at + 1 && letters_end - cursor > 2)
                    finish = letters_end;
                cursor = letters_end;
            } else if (letter(byte) || digit(byte) || byte == '-') {
                ++cursor;
            } else {
                break;
            }
        }
        if (finish != 0) {
            ranges.push_back({begin, finish - begin});
            begin = finish;
        } else {
            begin = at + 1;
        }
    }
    return ranges;
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
    std::vector<SourceRange> opaque_ranges;
    const auto collect_opaque = [&](const std::regex& pattern) {
        const auto ranges = scan_pattern_ranges(value, pattern);
        opaque_ranges.insert(opaque_ranges.end(), ranges.begin(), ranges.end());
    };
    collect_opaque(source.technical_url);
    collect_opaque(source.technical_utf8_email);
    collect_opaque(source.technical_email);
    const auto append_exact = [&](const std::regex& pattern, std::string_view expected) {
        using Iterator = std::regex_iterator<std::string_view::const_iterator>;
        for (Iterator it(value.begin(), value.end(), pattern), end; it != end; ++it) {
            const auto offset = static_cast<std::size_t>(it->position());
            const auto length = static_cast<std::size_t>(it->length());
            const auto overlaps_opaque = std::any_of(
                opaque_ranges.begin(), opaque_ranges.end(), [&](const SourceRange& range) {
                    return range.offset < offset + length && offset < range.offset + range.length;
                });
            if (overlaps_opaque)
                continue;
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
    std::sort(candidates.begin(),
              candidates.end(),
              [](const SourceRange& left, const SourceRange& right) {
                  return left.offset < right.offset ||
                         (left.offset == right.offset && left.length > right.length);
              });
    return candidates;
}

} // namespace tts_front::detail::technical
