#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/technical/admission.hpp"
#include "tts_front/technical/patterns.hpp"

#include <regex>
#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace tts_front;
    using namespace tts_front::detail;

    const auto check_email_scanner = [] {
        const auto& pattern = technical::patterns().technical_utf8_email;
        for (const auto* prefix : {"", " ", "_", "1", "x", ".", "+", "ж", "́"}) {
            for (const auto* local :
                 {"a", "a,b", "A_1%+-", "a.ж", "á", "a@b", "a..b", "1a", ",a", "a,b@c"}) {
                for (const auto* domain : {"example.com",
                                           ".ab",
                                           "..ab",
                                           "a.c",
                                           "a.co12",
                                           "a.co12.x",
                                           "a.co12.xyz4",
                                           "a-.COM",
                                           "a@b.com",
                                           "a.ж"}) {
                    for (const auto* tail : {"", ".", "́", "-", " a@b.cd", "@c.de"}) {
                        const auto input = std::string(prefix) + local + '@' + domain + tail;
                        std::vector<SourceRange> expected;
                        for (std::sregex_iterator it(input.begin(), input.end(), pattern), end;
                             it != end;
                             ++it)
                            expected.push_back({static_cast<std::size_t>(it->position()),
                                                static_cast<std::size_t>(it->length())});
                        const auto actual = technical::scan_pattern_ranges(input, pattern);
                        if (actual.size() != expected.size())
                            return false;
                        for (std::size_t index = 0; index < actual.size(); ++index)
                            if (actual[index].offset != expected[index].offset ||
                                actual[index].length != expected[index].length)
                                return false;
                    }
                }
            }
        }
        std::string local;
        for (int index = 0; index < 10000; ++index)
            local += "a,";
        local += "C++";
        if (!technical::scan_pattern_ranges(local, pattern).empty())
            return false;
        const auto spoken = technical::scan_pronunciation_candidates(local);
        if (spoken.size() != 1 || spoken.front().offset != local.size() - 3 ||
            spoken.front().length != 3)
            return false;
        const auto email = local + "@example.com";
        const auto ranges = technical::scan_protected_ranges(email);
        if (ranges.size() != 1 || ranges.front().offset != 0 ||
            ranges.front().length != email.size() ||
            !technical::scan_pronunciation_candidates(email).empty())
            return false;
        std::vector<ProtectedSpan> protected_spans;
        const auto protected_text =
            technical::protect(MappedText::from_original(email, nullptr), protected_spans);
        return protected_spans.size() == 1 && protected_spans.front().value == email &&
               technical::restore(protected_text, protected_spans).text == email;
    };

    const auto check = [](const std::string& input,
                          const std::size_t expected_offset,
                          const std::size_t expected_length) {
        std::vector<TextWarning> warnings;
        WarningSink warning_sink{warnings, {}};
        auto text = MappedText::from_original(input, &warning_sink.preserved_ranges);
        std::vector<ProtectedSpan> protected_spans;
        text =
            technical::protect_numeric_candidates(std::move(text), warning_sink, protected_spans);
        text = technical::protect(std::move(text), protected_spans);
        text = technical::restore(std::move(text), protected_spans);
        return text.text == input && warnings.size() == 1 &&
               warnings.front().code == WarningCode::UnresolvedNumber &&
               warnings.front().offset == expected_offset &&
               warnings.front().length == expected_length;
    };
    const auto check_protected = [](const std::string& input) {
        std::vector<TextWarning> warnings;
        WarningSink warning_sink{warnings, {}};
        auto text = MappedText::from_original(input, &warning_sink.preserved_ranges);
        std::vector<ProtectedSpan> protected_spans;
        text =
            technical::protect_numeric_candidates(std::move(text), warning_sink, protected_spans);
        text = technical::protect(std::move(text), protected_spans);
        text = technical::restore(std::move(text), protected_spans);
        return text.text == input && warnings.empty();
    };
    const auto check_warning_indexes = [] {
        std::vector<TextWarning> warnings{{WarningCode::UnresolvedNumber, "existing", 8, 2}};
        WarningSink warning_sink{warnings, {}};
        warning_sink.add_range(WarningCode::UnresolvedNumber, "duplicate", 8, 2);
        warning_sink.add_range(WarningCode::AmbiguousNormalization, "distinct code", 8, 2);
        warning_sink.preserve({20, 5});
        warning_sink.preserve({4, 4});
        warning_sink.preserve({7, 15});
        return warnings.size() == 2 && warning_sink.preserved_ranges.overlaps({3, 2}) &&
               warning_sink.preserved_ranges.overlaps({21, 1}) &&
               !warning_sink.preserved_ranges.overlaps({0, 4}) &&
               !warning_sink.preserved_ranges.overlaps({25, 1});
    };

    const std::string technical = "C++17,23% 456";
    const std::string ruble_suffix = "2.50 ₽";
    const std::string ruble_prefix = "₽2.50";
    return check(technical, 5, technical.size() - 5) &&
                   check(ruble_suffix, 0, ruble_suffix.size()) &&
                   check(ruble_prefix, 0, ruble_prefix.size()) &&
                   check_protected("https://example.com/2.50") &&
                   check_protected("user2.50@example.com") && check_warning_indexes() &&
                   check_email_scanner()
               ? 0
               : 1;
}
