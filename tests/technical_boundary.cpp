#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace tts_front;
    using namespace tts_front::detail;

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
                   check_protected("user2.50@example.com") && check_warning_indexes()
               ? 0
               : 1;
}
