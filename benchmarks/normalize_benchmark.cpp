#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/technical/admission.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

void benchmark_currency_prefix_scaling() {
    for (const unsigned int count : {1000u, 5000u, 10000u, 20000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 10);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "$-1 /2";
        }
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
        std::cout << count << " prefixed currency candidates, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/candidate\n";
        if (warnings.size() != count)
            std::cerr << "unexpected currency-prefix warning count: " << warnings.size() << "\n";
    }
}

bool benchmark_warning_sink_scaling() {
    constexpr std::array<unsigned int, 3> counts = {20000u, 100000u, 200000u};
    std::array<double, counts.size()> elapsed_seconds{};
    for (std::size_t sample = 0; sample < counts.size(); ++sample) {
        const auto count = counts[sample];
        std::vector<tts_front::TextWarning> warnings;
        warnings.reserve(count);
        tts_front::detail::WarningSink warning_sink{warnings, {}};
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned int index = 0; index < count; ++index) {
            const tts_front::detail::SourceRange source{static_cast<std::size_t>(index) * 4, 3};
            warning_sink.add(
                tts_front::WarningCode::UnresolvedNumber, "Malformed numeric candidate", source);
            warning_sink.add(tts_front::WarningCode::UnresolvedNumber,
                             "Duplicate malformed numeric candidate",
                             source);
        }
        elapsed_seconds[sample] =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " malformed candidate warnings, " << elapsed_seconds[sample] << " s, "
                  << (elapsed_seconds[sample] / static_cast<double>(count) * 1e6)
                  << " us/candidate\n";
        if (warnings.size() != count) {
            std::cerr << "unexpected deduplicated warning count: " << warnings.size() << "\n";
            return false;
        }
    }
    for (std::size_t sample = 1; sample < counts.size(); ++sample) {
        const auto input_ratio =
            static_cast<double>(counts[sample]) / static_cast<double>(counts[sample - 1]);
        const auto time_ratio = elapsed_seconds[sample] / elapsed_seconds[sample - 1];
        std::cout << "warning scaling slope " << counts[sample - 1] << "->" << counts[sample]
                  << ": " << time_ratio << "x time for " << input_ratio << "x candidates\n";
        if (time_ratio > input_ratio * 3.0) {
            std::cerr << "warning scaling exceeded the allowed slope\n";
            return false;
        }
    }
    return true;
}

std::size_t count_occurrences(const std::string_view text, const std::string_view needle) {
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string_view::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

bool benchmark_document_pipeline_scaling() {
    constexpr std::array<unsigned int, 3> counts = {250u, 1000u, 2000u};
    std::array<double, counts.size()> elapsed_seconds{};
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
    options.cleanup_spacing = false;
    constexpr std::string_view segment = "Сборка 2 kg RTX-4090 цена 2,500 руб. Use $1 #2.";
    for (std::size_t sample = 0; sample < counts.size(); ++sample) {
        const auto count = counts[sample];
        std::string text;
        text.reserve((segment.size() + 1) * count);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back('\n');
            text += segment;
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto result = frontend.process(text, options);
        elapsed_seconds[sample] =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " mixed/malformed document segments, " << elapsed_seconds[sample]
                  << " s, " << (elapsed_seconds[sample] / static_cast<double>(count) * 1e6)
                  << " us/segment\n";
        if (result.original_text != text || result.normalized_text.empty() ||
            count_occurrences(result.normalized_text, "kilogram") != count ||
            count_occurrences(result.normalized_text, "RTX-4090") != count) {
            std::cerr << "unexpected document-pipeline normalization result\n";
            return false;
        }
        const auto unresolved = static_cast<std::size_t>(
            std::count_if(result.warnings.begin(),
                          result.warnings.end(),
                          [](const tts_front::TextWarning& warning) {
                              return warning.code == tts_front::WarningCode::UnresolvedNumber;
                          }));
        const auto ambiguous = static_cast<std::size_t>(
            std::count_if(result.warnings.begin(),
                          result.warnings.end(),
                          [](const tts_front::TextWarning& warning) {
                              return warning.code == tts_front::WarningCode::AmbiguousNormalization;
                          }));
        if (unresolved != static_cast<std::size_t>(count) * 2 || ambiguous != 1) {
            std::cerr << "unexpected document-pipeline warning counts: " << unresolved
                      << " unresolved, " << ambiguous << " ambiguous\n";
            return false;
        }
    }
    for (std::size_t sample = 1; sample < counts.size(); ++sample) {
        const auto input_ratio =
            static_cast<double>(counts[sample]) / static_cast<double>(counts[sample - 1]);
        const auto time_ratio = elapsed_seconds[sample] / elapsed_seconds[sample - 1];
        std::cout << "document scaling slope " << counts[sample - 1] << "->" << counts[sample]
                  << ": " << time_ratio << "x time for " << input_ratio << "x segments\n";
        if (time_ratio > input_ratio * 3.0) {
            std::cerr << "document pipeline scaling exceeded the allowed slope\n";
            return false;
        }
    }
    return true;
}

void benchmark_russian_genitive_scaling() {
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.language = tts_front::Language::Russian;
    for (const unsigned int count : {1000u, 2000u, 5000u, 10000u, 40000u, 80000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 18);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "после 2 попыток";
        }
        const auto begin = std::chrono::steady_clock::now();
        const auto result = frontend.process(text, options);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " Russian genitive candidates, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/candidate\n";
        if (result.normalized_text.empty() || result.warnings.size() != 0)
            std::cerr << "unexpected genitive benchmark result\n";
    }
}

void benchmark_candidate_surface_scaling() {
    for (const unsigned int count : {5000u, 10000u, 20000u}) {
        const std::string text(count, '1');
        const tts_front::detail::Utf8Document document(text);
        const auto begin = std::chrono::steady_clock::now();
        const auto surface = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Numeric);
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " candidate-surface digits, " << elapsed << " s, "
                  << (elapsed / static_cast<double>(count) * 1e6) << " us/digit\n";
        if (!surface.valid() || surface.end != document.points.size())
            std::cerr << "unexpected candidate surface boundary\n";
    }
}

void benchmark_mixed_candidate_scaling() {
    tts_front::TextFrontend frontend;
    tts_front::TextFrontendOptions options;
    options.mixed_language_policy = tts_front::MixedLanguagePolicy::SegmentCandidates;
    for (const unsigned int count : {1000u, 5000u, 10000u, 20000u}) {
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 40);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "сборка ";
            text += std::to_string(index % 10 + 1);
            text += " kg Build ";
            text += std::to_string((index + 1) % 10 + 1);
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
        if (result.normalized_text.empty() ||
            result.normalized_text.find("kilogram") == std::string::npos)
            std::cerr << "unexpected mixed typed-candidate normalization result\n";
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

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--currency-prefix-scaling") {
        benchmark_currency_prefix_scaling();
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--warning-scaling")
        return benchmark_warning_sink_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--document-scaling")
        return benchmark_document_pipeline_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
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
    benchmark_currency_prefix_scaling();
    benchmark_russian_genitive_scaling();
    benchmark_candidate_surface_scaling();
    benchmark_mixed_candidate_scaling();
    benchmark_technical_heavy_scaling();
}
