#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace tts_front;
    using namespace tts_front::detail;

    const std::string input = "C++17,23% 456";
    std::vector<TextWarning> warnings;
    WarningSink warning_sink{warnings, {}};
    auto text = MappedText::from_original(input, &warning_sink.preserved_ranges);
    std::vector<ProtectedSpan> protected_spans;
    text = technical::protect_numeric_candidates(std::move(text), warning_sink, protected_spans);
    text = technical::protect(std::move(text), protected_spans);
    text = technical::restore(std::move(text), protected_spans);
    return text.text == input && !warnings.empty() ? 0 : 1;
}
