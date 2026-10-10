#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/technical/admission.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

void benchmark_numeric_scaling() {
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.language = tts_front::Language::English;
    for (const auto count : {1000u, 5000u, 10000u, 20000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 2);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text.push_back('1');
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto result = frontend.process(text, options);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " numeric tokens, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/token\n";
        if (result.normalized_text.empty())
            std::cerr << "unexpected empty normalization result\n";
    }
}

void benchmark_technical_contiguous_numeric_scaling() {
    for (const unsigned int count : {5000u, 10000u, 20000u}) {
        const std::string text(count, '1');
        std::vector<tts_front::TextWarning> warnings;
        tts_front::detail::WarningSink warning_sink{warnings, {}};
        auto mapped =
            tts_front::detail::MappedText::from_original(text, &warning_sink.preserved_ranges);
        std::vector<tts_front::detail::ProtectedSpan> protected_spans;
        const auto begin = std::chrono::steady_clock::now();
        mapped = tts_front::detail::technical::protect_numeric_candidates(
            std::move(mapped), warning_sink, protected_spans);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " contiguous technical digits, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/digit\n";
        if (mapped.text != text)
            std::cerr << "unexpected technical admission edit\n";
    }
}

void benchmark_mixed_candidate_scaling() {
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
    for (const unsigned int count : {100u, 500u, 1000u, 2000u, 20000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 8);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "тест ";
            text += std::to_string(index % 10 + 1);
            text += " kg";
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto result = frontend.process(text, options);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " mixed candidates, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/candidate\n";
        if (count == 20000)
            std::cout << "mixed warnings: " << result.warnings.size() << "\n";
        if (result.normalized_text.empty())
            std::cerr << "unexpected empty mixed normalization result\n";
    }
}

void benchmark_technical_heavy_scaling() {
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
    options.cleanup_spacing = false;
    for (const unsigned int count : {1000u, 5000u, 10000u, 20000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 24);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "тест RTX ";
            text += std::to_string(index % 4096);
            text += " 2 kg";
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto result = frontend.process(text, options);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " technical-heavy candidates, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/candidate\n";
        if (result.normalized_text.empty())
            std::cerr << "unexpected empty technical-heavy result\n";
    }
}

int main() {
    const std::string text = "В 2026 году GPU RTX 4090 обработал 12500 запросов.";
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.language = tts_front::Language::Russian;
    constexpr int iterations = 10000;
    const auto begin = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
        (void)frontend.process(text, options);
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << iterations << " iterations, " << elapsed << " s, " << (elapsed / iterations * 1e6)
              << " us/request\n";
    benchmark_numeric_scaling();
    benchmark_technical_contiguous_numeric_scaling();
    benchmark_mixed_candidate_scaling();
    benchmark_technical_heavy_scaling();
}
