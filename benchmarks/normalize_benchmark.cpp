#include "tts_front.hpp"
#include "tts_front/core/mapped_text.hpp"
#include "tts_front/core/normalization_support.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/utf8_document.hpp"
#include "tts_front/normalization/mixed_language.hpp"
#include "tts_front/technical/admission.hpp"
#include "tts_front/technical/patterns.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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

bool benchmark_technical_ownership_scaling() {
    constexpr std::array<unsigned int, 4> counts = {1000u, 5000u, 10000u, 20000u};
    std::array<double, counts.size()> elapsed_seconds{};
    for (std::size_t sample = 0; sample < counts.size(); ++sample) {
        const auto count = counts[sample];
        std::string text;
        text.reserve(static_cast<std::size_t>(count) * 24);
        for (unsigned int index = 0; index < count; ++index) {
            if (index != 0)
                text.push_back(' ');
            text += "Тест RTX 4090 and";
        }
        const auto technical_index = tts_front::detail::build_technical_range_index(text);
        const auto candidates = tts_front::detail::technical::scan_pronunciation_candidates(text);
        const auto begin = std::chrono::steady_clock::now();
        const auto owner_index =
            tts_front::detail::build_local_language_owner_index(text, technical_index);
        std::size_t owned = 0;
        for (const auto& candidate : candidates) {
            if (tts_front::detail::detect_local_language(
                    candidate.offset, candidate.offset + candidate.length, owner_index))
                ++owned;
        }
        elapsed_seconds[sample] =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << count << " technical ownership candidates, " << elapsed_seconds[sample]
                  << " s, " << (elapsed_seconds[sample] / static_cast<double>(count) * 1e6)
                  << " us/candidate\n";
        if (candidates.size() != count || owned != count) {
            std::cerr << "unexpected technical ownership result\n";
            return false;
        }
    }
    for (std::size_t sample = 1; sample < counts.size(); ++sample) {
        const auto input_ratio =
            static_cast<double>(counts[sample]) / static_cast<double>(counts[sample - 1]);
        const auto time_ratio = elapsed_seconds[sample] / elapsed_seconds[sample - 1];
        std::cout << "technical ownership scaling " << counts[sample - 1] << "->" << counts[sample]
                  << ": " << time_ratio << "x time for " << input_ratio << "x candidates\n";
        if (time_ratio > input_ratio * 3.0) {
            std::cerr << "technical ownership scaling exceeded the allowed slope\n";
            return false;
        }
    }
    return true;
}

bool benchmark_cardinal_source_scaling() {
    using namespace tts_front::detail;
    constexpr std::array<unsigned int, 4> counts = {800u, 1600u, 3200u, 6400u};
    constexpr std::string_view phrase = "2 задачи";
    for (const auto connector : {std::string_view(" / "),
                                 std::string_view(" # "),
                                 std::string_view("./ "),
                                 std::string_view(":# "),
                                 std::string_view(",% "),
                                 std::string_view("; ")}) {
        std::array<MappedText, counts.size()> sources;
        for (std::size_t size = 0; size < counts.size(); ++size) {
            const auto count = counts[size];
            std::string text;
            text.reserve((phrase.size() + connector.size()) * count);
            for (unsigned int item = 0; item < count; ++item) {
                if (item != 0)
                    text += connector;
                text += phrase;
            }
            sources[size] = MappedText::from_original(text, nullptr);
        }
        std::array<double, counts.size()> timings{};
        // Largest first: warm large allocations before comparing smaller inputs.
        for (std::size_t size = counts.size(); size-- > 0;) {
            const auto count = counts[size];
            const auto& source = sources[size];
            const auto expected = connector == "; " || connector == ",% " ? count : 1u;
            const SourceRange last{source.text.size() - phrase.size(), 1};
            const auto warm = build_cardinal_source_index(source);
            if (warm.ranges.size() != expected || !warm.contains(last)) {
                std::cerr << "unexpected cardinal source index\n";
                return false;
            }
            std::array<double, 3> samples{};
            for (auto& sample : samples) {
                const auto begin = std::chrono::steady_clock::now();
                unsigned int repetitions = 0;
                double elapsed = 0;
                do {
                    const auto index = build_cardinal_source_index(source);
                    if (index.ranges.size() != expected || !index.contains(last)) {
                        std::cerr << "unexpected cardinal source index\n";
                        return false;
                    }
                    ++repetitions;
                    elapsed =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin)
                            .count();
                } while (elapsed < 0.04);
                sample = elapsed / repetitions;
            }
            std::sort(samples.begin(), samples.end());
            timings[size] = samples[1];
        }
        double previous = 0;
        double log_slope = 0;
        for (std::size_t size = 0; size < counts.size(); ++size) {
            const auto count = counts[size];
            const auto elapsed = timings[size];
            std::cout << count << " cardinal origins, connector [" << connector << "], " << elapsed
                      << " s, slope " << (previous > 0 ? elapsed / previous : 0) << "x\n";
            // Log-time fit at x=0,1,2,3: mean x=1.5, sum of squared deviations=5.
            log_slope += (static_cast<double>(size) - 1.5) * std::log(elapsed) / 5.0;
            previous = elapsed;
        }
        const auto doubling_slope = std::exp(log_slope);
        std::cout << "cardinal source fitted doubling slope: " << doubling_slope << "x\n";
        if (doubling_slope > 3.25) {
            std::cerr << "cardinal source index exceeded the allowed doubling slope\n";
            return false;
        }
    }
    return true;
}

bool benchmark_ascii_email_scaling() {
    using namespace tts_front::detail;
    constexpr std::array<unsigned int, 4> counts = {1000u, 2000u, 4000u, 8000u};
    const auto& pattern = technical::patterns().technical_email;
    for (const auto suffix :
         {std::string_view(""), std::string_view("@localhost"), std::string_view("@example.com")}) {
        std::array<double, counts.size()> timings{};
        for (std::size_t size = counts.size(); size-- > 0;) {
            const auto text = std::string(counts[size], '7') + std::string(suffix);
            const std::size_t expected = suffix == "@example.com" ? 1 : 0;
            const auto warm = technical::scan_pattern_ranges(text, pattern);
            if (warm.size() != expected ||
                (expected != 0 &&
                 (warm.front().offset != 0 || warm.front().length != text.size()))) {
                std::cerr << "unexpected ASCII email ranges\n";
                return false;
            }
            std::array<double, 3> samples{};
            for (auto& sample : samples) {
                const auto begin = std::chrono::steady_clock::now();
                unsigned int repetitions = 0;
                double elapsed = 0;
                do {
                    const auto ranges = technical::scan_pattern_ranges(text, pattern);
                    if (ranges.size() != expected ||
                        (expected != 0 &&
                         (ranges.front().offset != 0 || ranges.front().length != text.size()))) {
                        std::cerr << "unexpected ASCII email ranges\n";
                        return false;
                    }
                    ++repetitions;
                    elapsed =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin)
                            .count();
                } while (elapsed < 0.04);
                sample = elapsed / repetitions;
            }
            std::sort(samples.begin(), samples.end());
            timings[size] = samples[1];
        }
        double log_slope = 0;
        for (std::size_t size = 0; size < counts.size(); ++size) {
            std::cout << counts[size] << " ASCII email local bytes, suffix [" << suffix << "], "
                      << timings[size] << " s\n";
            log_slope += (static_cast<double>(size) - 1.5) * std::log(timings[size]) / 5.0;
        }
        const auto doubling_slope = std::exp(log_slope);
        std::cout << "ASCII email fitted doubling slope: " << doubling_slope << "x\n";
        if (doubling_slope > 3.25) {
            std::cerr << "ASCII email scan exceeded the allowed doubling slope\n";
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--ascii-email-scaling")
        return benchmark_ascii_email_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--currency-prefix-scaling") {
        benchmark_currency_prefix_scaling();
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--warning-scaling")
        return benchmark_warning_sink_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--document-scaling")
        return benchmark_document_pipeline_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--cardinal-source-scaling")
        return benchmark_cardinal_source_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--technical-ownership-scaling")
        return benchmark_technical_ownership_scaling() ? EXIT_SUCCESS : EXIT_FAILURE;
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
    if (!benchmark_technical_ownership_scaling())
        return EXIT_FAILURE;
}
