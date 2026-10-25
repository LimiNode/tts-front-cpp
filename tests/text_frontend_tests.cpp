#include "tts_front.hpp"
#include "tts_front/core/numeric_scanner.hpp"
#include "tts_front/core/utf8_document.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

using namespace tts_front;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__   \
                      << "\n";                                                                     \
            return EXIT_FAILURE;                                                                   \
        }                                                                                          \
    } while (false)

int main() {
    TextFrontend frontend;
    TextFrontendOptions ru;
    ru.language = Language::Russian;
    {
        const tts_front::detail::Utf8Document document("+1e+3#4");
        const auto surface = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Technical);
        CHECK(surface.valid());
        CHECK(surface.numeric.exponent == tts_front::detail::ExponentState::Complete);
        CHECK(surface.end == document.points.size());
    }
    {
        const tts_front::detail::Utf8Document document("2 kg#3");
        const auto candidate = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
        const auto admission = tts_front::detail::scan_admission(
            document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.begin == 0);
        CHECK(admission.kind == tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.base_end == candidate.end);
        CHECK(admission.outcome == tts_front::detail::AdmissionOutcome::Attached);
        CHECK(admission.extended());
        CHECK(admission.end == document.points.size());

        const auto typed = tts_front::detail::scan_admission(
            document, 4, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(typed.begin == 4);
        CHECK(typed.base_end == 4);
        CHECK(typed.outcome == tts_front::detail::AdmissionOutcome::Attached);
        CHECK(typed.end == document.points.size());
    }
    {
        const tts_front::detail::Utf8Document document("2 kg 3");
        const auto candidate = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
        const auto admission = tts_front::detail::scan_admission(
            document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.begin == 0);
        CHECK(admission.base_end == candidate.end);
        CHECK(admission.outcome == tts_front::detail::AdmissionOutcome::None);
        CHECK(!admission.extended());
        CHECK(admission.end == candidate.end);
        CHECK(admission.end < document.points.size());
    }
    {
        const tts_front::detail::Utf8Document document("2 kg #3");
        const auto candidate = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
        const auto admission = tts_front::detail::scan_admission(
            document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.outcome == tts_front::detail::AdmissionOutcome::Spaced);
        CHECK(admission.extended());
        CHECK(admission.end == document.points.size());
    }
    {
        const tts_front::detail::Utf8Document document("2...+ 3");
        const auto candidate = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
        const auto admission = tts_front::detail::scan_admission(
            document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.outcome == tts_front::detail::AdmissionOutcome::Punctuated);
        CHECK(admission.extended());
        CHECK(admission.end == document.points.size());
    }
    {
        const tts_front::detail::Utf8Document document("2 kg #3.../ 4");
        const auto candidate = tts_front::detail::scan_candidate_surface(
            document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
        const auto admission = tts_front::detail::scan_admission(
            document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
        CHECK(admission.outcome == tts_front::detail::AdmissionOutcome::Compound);
        CHECK(admission.extended());
        CHECK(admission.end == document.points.size());
    }
    {
        const std::array<std::pair<std::string, tts_front::detail::AdmissionOutcome>, 3>
            malformed_combining = {{{std::string("2") + "\xCC\x81" + "%",
                                     tts_front::detail::AdmissionOutcome::Attached},
                                    {std::string("2") + "\xCC\x81" + " %",
                                     tts_front::detail::AdmissionOutcome::Spaced},
                                    {std::string("2") + "\xCC\x81" + " % 3",
                                     tts_front::detail::AdmissionOutcome::Compound}}};
        for (const auto& [input, expected] : malformed_combining) {
            const tts_front::detail::Utf8Document document(input);
            const auto candidate = tts_front::detail::scan_candidate_surface(
                document, 0, tts_front::detail::CandidateSurfaceKind::Contextual);
            CHECK(candidate.numeric.malformed_combining);
            const auto admission = tts_front::detail::scan_admission(
                document, candidate, tts_front::detail::CandidateSurfaceKind::Contextual);
            CHECK(admission.outcome == expected);
            CHECK(admission.extended());
            CHECK(admission.end == document.points.size());
        }
    }
    CHECK(frontend.process("У меня 42 яблока.", ru).normalized_text == "У меня сорок два яблока.");
    CHECK(frontend.process("1% 2% 5% 11% 21%", ru).normalized_text ==
          "один процент два процента пять процентов одиннадцать процентов двадцать один процент");
    CHECK(frontend.process("1:01 2:02 5:05", ru).normalized_text ==
          "один час одна минута два часа две минуты пять часов пять минут");
    CHECK(frontend.process("1 руб. 2 руб. 5 руб.", ru).normalized_text ==
          "один рубль два рубля пять рублей.");
    CHECK(frontend.process("7,5 12 500 -42", ru).normalized_text ==
          "семь целых пять десятых двенадцать тысяч пятьсот минус сорок два");
    CHECK(frontend.process("В 2026 г.", ru).normalized_text == "В две тысячи двадцать шестом году");
    CHECK(frontend.process("2 мм 2 м 2 метра", ru).normalized_text ==
          "два миллиметра два метра два метра");
    CHECK(frontend.process("И т.д. и т.п.", ru).normalized_text == "И так далее и тому подобное");
    CHECK(frontend.process("5 руб. 5 рублей", ru).normalized_text == "пять рублей пять рублей");
    CHECK(frontend.process("21:21 22:22", ru).normalized_text ==
          "двадцать один час двадцать одна минута двадцать два часа двадцать две минуты");
    CHECK(frontend.process("21000 22000 11000", ru).normalized_text ==
          "двадцать одна тысяча двадцать две тысячи одиннадцать тысяч");
    CHECK(frontend.process("В 1999 г. В 2000 г. В 2012 г. В 2037 г.", ru).normalized_text ==
          "В тысяча девятьсот девяносто девятом году В двухтысячном году В две тысячи двенадцатом "
          "году В две тысячи тридцать седьмом году");
    const std::array<std::pair<std::string, std::string>, 12> RussianLocativeYears = {{
        {"в 1000 году", "в тысячном году"},
        {"в 1899 году", "в тысяча восемьсот девяносто девятом году"},
        {"в 1900 году", "в тысяча девятисотом году"},
        {"в 1999 году", "в тысяча девятьсот девяносто девятом году"},
        {"в 2000 году", "в двухтысячном году"},
        {"в 2001 году", "в две тысячи первом году"},
        {"в 2012 году", "в две тысячи двенадцатом году"},
        {"в 2020 году", "в две тысячи двадцатом году"},
        {"в 2024 году", "в две тысячи двадцать четвёртом году"},
        {"в 2026 году", "в две тысячи двадцать шестом году"},
        {"в 2042 году", "в две тысячи сорок втором году"},
        {"в 9999 году", "в девять тысяч девятьсот девяносто девятом году"},
    }};
    for (const auto& [input, expected] : RussianLocativeYears) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == expected);
        CHECK(result.pronunciation_text == expected);
        CHECK(result.warnings.empty());
    }
    const std::array<std::pair<std::string, std::string>, 12> RussianAccusativeYears = {{
        {"за 1000 год", "за тысячный год"},
        {"за 1899 год", "за тысяча восемьсот девяносто девятый год"},
        {"За 1900 год", "За тысяча девятисотый год"},
        {"за 1999 год", "за тысяча девятьсот девяносто девятый год"},
        {"за 2000 год", "за двухтысячный год"},
        {"за 2001 год", "за две тысячи первый год"},
        {"за 2012 год", "за две тысячи двенадцатый год"},
        {"за 2020 год", "за две тысячи двадцатый год"},
        {"за 2024 год", "за две тысячи двадцать четвёртый год"},
        {"за 2026 год", "за две тысячи двадцать шестой год"},
        {"за 2042 год", "за две тысячи сорок второй год"},
        {"за 9999 год", "за девять тысяч девятьсот девяносто девятый год"},
    }};
    for (const auto& [input, expected] : RussianAccusativeYears) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == expected);
        CHECK(result.pronunciation_text == expected);
        CHECK(result.warnings.empty());
    }
    CHECK(frontend.process("1999", ru).normalized_text == "одна тысяча девятьсот девяносто девять");
    CHECK(frontend.process("В 1900 году", ru).normalized_text == "В тысяча девятисотом году");
    CHECK(frontend.process("архив 2024 года", ru).normalized_text ==
          "архив две тысячи двадцать четыре года");
    const auto lexical_year_neighbor = frontend.process("архив 1999 году", ru);
    CHECK(lexical_year_neighbor.normalized_text ==
          "архив одна тысяча девятьсот девяносто девять году");
    CHECK(lexical_year_neighbor.warnings.empty());
    TextFrontendOptions russian_year_auto = ru;
    russian_year_auto.language = Language::Auto;
    TextFrontendOptions russian_year_segmented = russian_year_auto;
    russian_year_segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
    const std::array<TextFrontendOptions, 3> RussianYearModes = {
        ru, russian_year_auto, russian_year_segmented};
    for (const auto& options : RussianYearModes) {
        const auto positive = frontend.process("в 2024 году.", options);
        CHECK(positive.normalized_text == "в две тысячи двадцать четвёртом году.");
        CHECK(positive.pronunciation_text == positive.normalized_text);
        CHECK(positive.warnings.empty());

        const auto independent = frontend.process("в 1999 году 2000", options);
        CHECK(independent.normalized_text ==
              "в тысяча девятьсот девяносто девятом году две тысячи");
        CHECK(independent.warnings.empty());

        const auto grouped = frontend.process("в 1 999 году", options);
        CHECK(grouped.normalized_text == "в тысяча девятьсот девяносто девятом году");
        CHECK(grouped.pronunciation_text == grouped.normalized_text);
        CHECK(grouped.warnings.empty());

        for (const auto& input : {std::string("в 1999 году#4"),
                                  std::string("за 2001 год/4"),
                                  std::string("с 1999 по 2001 год"),
                                  std::string("с 1999 по 2001 год#4"),
                                  std::string("в 0999 году"),
                                  std::string("за 0999 год"),
                                  std::string("в +1999 году"),
                                  std::string("в -1999 году"),
                                  std::string("в −1999 году"),
                                  std::string("в 1999год"),
                                  std::string("в 1999 годуfoo"),
                                  std::string("в 1999 годуархив"),
                                  std::string("в 1999 году") + "\xCC\x81",
                                  std::string("в 1999 году") + "\xCC\x81" + "foo"}) {
            const auto result = frontend.process(input, options);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved != result.warnings.end());
            CHECK(std::count_if(result.warnings.begin(),
                                result.warnings.end(),
                                [](const TextWarning& warning) {
                                    return warning.code == WarningCode::UnresolvedNumber;
                                }) == 1);
            CHECK(std::all_of(
                result.warnings.begin(), result.warnings.end(), [&](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber ||
                           (warning.code == WarningCode::AmbiguousNormalization &&
                            warning.offset == 0 && warning.length == input.size());
                }));
            auto candidate_begin = input.find_first_of("0123456789");
            for (const auto& sign : {std::string("+"), std::string("-"), std::string("−")}) {
                const auto sign_begin = input.find(sign);
                if (sign_begin != std::string::npos)
                    candidate_begin = std::min(candidate_begin, sign_begin);
            }
            CHECK(unresolved->offset == candidate_begin);
            CHECK(unresolved->length == input.size() - candidate_begin);
        }
    }
    for (const auto& options : RussianYearModes) {
        const std::array<std::pair<std::string, std::string>, 3> canonical_date_years = {{
            {"Дата 01.01.2004.", "Дата первое января две тысячи четвёртого года."},
            {"Дата 01.01.2024.", "Дата первое января две тысячи двадцать четвёртого года."},
            {"Дата 01.01.2044.", "Дата первое января две тысячи сорок четвёртого года."},
        }};
        for (const auto& [input, expected] : canonical_date_years) {
            const auto result = frontend.process(input, options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == expected);
            CHECK(result.pronunciation_text == expected);
            CHECK(result.warnings.empty());
        }

        const std::array<std::pair<std::string, std::string>, 3> unchanged_date_years = {{
            {"Дата 31.12.1999.",
             "Дата тридцать первое декабря тысяча девятьсот девяносто девятого года."},
            {"Дата 01.01.2000.", "Дата первое января двухтысячного года."},
            {"Дата 01.01.2025.", "Дата первое января две тысячи двадцать пятого года."},
        }};
        for (const auto& [input, expected] : unchanged_date_years) {
            const auto result = frontend.process(input, options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == expected);
            CHECK(result.pronunciation_text == expected);
            CHECK(result.warnings.empty());
        }

        const auto surface_local = frontend.process("Подтвердите дату 01.01.2024.", options);
        CHECK(surface_local.normalized_text ==
              "Подтвердите дату первое января две тысячи двадцать четвёртого года.");
        CHECK(surface_local.pronunciation_text == surface_local.normalized_text);
        CHECK(surface_local.warnings.empty());

        for (const auto& surface :
             {std::string("29.02.2025"), std::string("31.04.2024"), std::string("32.01.2024")}) {
            const auto input = "Дата " + surface + ".";
            const auto result = frontend.process(input, options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved != result.warnings.end());
            CHECK(std::count_if(result.warnings.begin(),
                                result.warnings.end(),
                                [](const TextWarning& warning) {
                                    return warning.code == WarningCode::UnresolvedNumber;
                                }) == 1);
            CHECK(std::all_of(
                result.warnings.begin(), result.warnings.end(), [&](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber ||
                           (warning.code == WarningCode::AmbiguousNormalization &&
                            warning.offset == 0 && warning.length == input.size());
                }));
            CHECK(unresolved->offset == input.find(surface));
            CHECK(unresolved->length == surface.size());
        }

        const std::array<std::string, 13> unsupported_date_surfaces = {
            "01.01.2024foo",
            "01.01.2024слово",
            "01.01.2024#4",
            "01.01.2024/4",
            "01.01.2024%4",
            "01.01.2024+4",
            "01.01.2024−4",
            std::string("01.01.2024") + "\xCC\x81",
            std::string("01.01.2024") + "\xCC\x81" + "foo",
            "+01.01.2024#4",
            "-01.01.2024/4",
            "−01.01.2024%4",
            "01.01.2024% 4",
        };
        for (const auto& surface : unsupported_date_surfaces) {
            const auto input = "Дата " + surface;
            const auto result = frontend.process(input, options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved != result.warnings.end());
            CHECK(std::count_if(result.warnings.begin(),
                                result.warnings.end(),
                                [](const TextWarning& warning) {
                                    return warning.code == WarningCode::UnresolvedNumber;
                                }) == 1);
            CHECK(std::all_of(
                result.warnings.begin(), result.warnings.end(), [&](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber ||
                           (warning.code == WarningCode::AmbiguousNormalization &&
                            warning.offset == 0 && warning.length == input.size());
                }));
            CHECK(unresolved->offset == input.find(surface));
            CHECK(unresolved->length == surface.size());
        }

        const auto independent = frontend.process("Дата 01.01.2024 7", options);
        CHECK(independent.normalized_text ==
              "Дата первое января две тысячи двадцать четвёртого года семь");
        CHECK(independent.pronunciation_text == independent.normalized_text);
        CHECK(independent.warnings.empty());
    }
    CHECK(frontend.process("01.02.2026", ru).normalized_text ==
          "первое февраля две тысячи двадцать шестого года");
    CHECK(frontend.process("1,1 2,2 5,1", ru).normalized_text ==
          "одна целая одна десятая две целых две десятых пять целых одна десятая");
    TextFrontendOptions en;
    en.language = Language::English;
    {
        const std::array<std::tuple<std::string, std::string, std::string>, 5> years = {{
            {"1900", "one thousand nine hundred", "nineteen hundred"},
            {"1999", "one thousand nine hundred ninety nine", "nineteen ninety nine"},
            {"2000", "two thousand", "two thousand"},
            {"2001", "two thousand one", "two thousand one"},
            {"2024", "two thousand twenty four", "two thousand twenty four"},
        }};
        for (const auto& [surface, cardinal, spoken] : years) {
            CHECK(frontend.process("in " + surface, en).normalized_text == "in " + spoken);
            CHECK(frontend.process("from " + surface, en).normalized_text == "from " + spoken);
            CHECK(frontend.process(surface, en).normalized_text == cardinal);
        }
        for (const auto& [context, surface, spoken] :
             {std::tuple<std::string, std::string, std::string>{"In", "1900", "nineteen hundred"},
              {"From", "1999", "nineteen ninety nine"},
              {"In", "2024", "two thousand twenty four"}}) {
            const auto result = frontend.process(context + " " + surface, en);
            CHECK(result.normalized_text == context + " " + spoken);
            CHECK(result.pronunciation_text == context + " " + spoken);
            CHECK(result.warnings.empty());
        }
        const std::array<std::pair<std::string, std::string>, 5> dates = {{
            {"January 1, 1900", "January first, nineteen hundred"},
            {"January 1, 1999", "January first, nineteen ninety nine"},
            {"January 1, 2000", "January first, two thousand"},
            {"January 1, 2001", "January first, two thousand one"},
            {"January 1, 2024", "January first, two thousand twenty four"},
        }};
        for (const auto& [surface, spoken] : dates) {
            const auto result = frontend.process(surface, en);
            CHECK(result.normalized_text == spoken);
            CHECK(result.pronunciation_text == spoken);
            CHECK(result.warnings.empty());
        }
        CHECK(frontend.process("between 1999 and 2001", en).normalized_text ==
              "between one thousand nine hundred ninety nine and two thousand one");
        const auto range = frontend.process("from 1999 to 2001", en);
        CHECK(range.normalized_text == "from 1999 to 2001");
        CHECK(range.pronunciation_text == "from 1999 to 2001");
        CHECK(range.warnings.size() == 1);
        CHECK(range.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(range.warnings.front().offset == 5);
        CHECK(range.warnings.front().length == 12);
        CHECK(frontend.process("in 2100", en).normalized_text == "in two thousand one hundred");
        CHECK(frontend.process("in 999", en).normalized_text == "in nine hundred ninety nine");
    }
    for (const auto& input : {std::string("in 1999#4"),
                              std::string("from 2024/4"),
                              std::string("in 1999%foo"),
                              std::string("in 1999foo"),
                              std::string("in 1999+4")}) {
        const auto result = frontend.process(input, en);
        const auto number_offset =
            input.find("1999") != std::string::npos ? input.find("1999") : input.find("2024");
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == number_offset);
        CHECK(result.warnings.front().length == input.size() - number_offset);
    }
    TextFrontendOptions auto_options = en;
    auto_options.language = Language::Auto;
    TextFrontendOptions segmented = en;
    segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
    CHECK(frontend.process("7.5%", en).normalized_text == "seven point five percent");
    CHECK(frontend.process("-0 -0.0 -0%", en).normalized_text ==
          "minus zero minus zero point zero minus zero percent");
    CHECK(frontend.process("-0 -0,00 -0%", ru).normalized_text ==
          "минус ноль минус ноль целых ноль сотых минус ноль процентов");
    {
        const std::string unsupported_numeric[] = {"1%word",
                                                   "1%2",
                                                   "1 %word",
                                                   "1 %2",
                                                   "1e+3",
                                                   "1.2e-3",
                                                   "1.2#3",
                                                   "1.2$3",
                                                   "123#abc",
                                                   "123$abc",
                                                   "1#abc",
                                                   "1$abc",
                                                   "1*2",
                                                   "1&2",
                                                   "1^2",
                                                   std::string("10") + "\xE2\x80\x93" + "20",
                                                   std::string("10") + "\xE2\x80\x94" + "20",
                                                   std::string("10") + "\xE2\x88\x92" + "20",
                                                   std::string("10 ") + "\xE2\x80\x93" + " 20",
                                                   std::string("10 ") + "\xE2\x80\x94" + " 20",
                                                   "+7 (999) 123-45-67"};
        for (const auto& input : unsupported_numeric) {
            const auto result = frontend.process(input, en);
            CHECK(result.normalized_text == input);
            CHECK(!result.warnings.empty());
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        }
    }
    CHECK(frontend.process("1 %", en).normalized_text == "one percent");
    const std::string russian_percent_suffix =
        std::string("1,2 %") + "\xD1\x81\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xBE";
    {
        const auto result = frontend.process(russian_percent_suffix, ru);
        CHECK(result.normalized_text == russian_percent_suffix);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    CHECK(frontend.process("1,234 12,345,678", en).normalized_text ==
          "one thousand two hundred thirty four twelve million three hundred forty five thousand "
          "six hundred seventy eight");
    CHECK(frontend.process("1,234.56", en).normalized_text ==
          "one thousand two hundred thirty four point five six");
    CHECK(frontend.process("1,234% 1,234.56%", en).normalized_text ==
          "one thousand two hundred thirty four percent one thousand two hundred thirty four "
          "point five six percent");
    CHECK(frontend.process("$1,234.56", en).normalized_text ==
          "one thousand two hundred thirty four dollars fifty six cents");
    CHECK(frontend.process("abc1,234 1,23 1,2345", en).normalized_text == "abc1,234 1,23 1,2345");
    for (const auto& input : {std::string("1,23"), std::string("1,2345")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    {
        const auto malformed_group = frontend.process("1,234.56.7", en);
        CHECK(malformed_group.normalized_text == "1,234.56.7");
        CHECK(!malformed_group.warnings.empty());
        CHECK(malformed_group.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    {
        const std::string oversized_group = "x 99,999,999,999,999,999,999";
        const auto result = frontend.process(oversized_group, en);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 2);
        CHECK(result.warnings.front().length == oversized_group.size() - 2);
    }
    CHECK(frontend.process("$12.50 $12.05 $12.5 $1 $2", en).normalized_text ==
          "twelve dollars fifty cents twelve dollars five cents twelve dollars fifty cents one "
          "dollar two dollars");
    CHECK(
        frontend.process("RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2", en).normalized_text ==
        "RTX 4090 CUDA 13.3 v2.1.0 127.0.0.1 C++ C# HTTP/2");
    CHECK(frontend.process("7.05 7.50", en).normalized_text ==
          "seven point zero five seven point five zero");
    CHECK(frontend.process("1:01 2:02 12:30", en).normalized_text ==
          "one hour one minute two hours two minutes twelve hours thirty minutes");
    CHECK(frontend.process("1 kg 2 km", en).normalized_text == "one kilogram two kilometers");
    CHECK(frontend.process("The retry threshold is (2).", en).normalized_text ==
          "The retry threshold is (two).");
    CHECK(frontend.process("Price ($12.50) includes tax.", en).normalized_text ==
          "Price (twelve dollars fifty cents) includes tax.");
    CHECK(frontend.process("Цена (2 рубля) указана без скидки.", ru).normalized_text ==
          "Цена (два рубля) указана без скидки.");
    {
        const auto result = frontend.process("1.2)", en);
        CHECK(result.normalized_text == "1.2)");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == std::string("1.2)").size());
    }
    for (const auto& input : {std::string("12 3456"),
                              std::string("12 345foo"),
                              std::string("12 345:6"),
                              std::string("12 345 678 901"),
                              std::string("1,234,567,890"),
                              std::string("1.2..3"),
                              std::string("1.2.,3"),
                              std::string("1:02..3"),
                              std::string("1:02::3"),
                              std::string("1:02%word"),
                              std::string("$ 1"),
                              std::string("$-1"),
                              std::string("€1"),
                              std::string("1:02#3")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().length == input.size());
    }
    for (const auto& input : {std::string("12 3456"),
                              std::string("12 345foo"),
                              std::string("12 345:6"),
                              std::string("12 345 678 901"),
                              std::string("1.2.,3"),
                              std::string("1.2..3"),
                              std::string("1:02::3"),
                              std::string("1:02%word"),
                              std::string("1:02..3"),
                              std::string("$ 1"),
                              std::string("$-1"),
                              std::string("\xE2\x82\xAC") + "1",
                              std::string("1:02#3")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    CHECK(frontend.process("1 kg -2 kg", en).normalized_text == "one kilogram minus two kilograms");
    CHECK(frontend.process("1 кг -2 кг", ru).normalized_text ==
          "один килограмм минус два килограмма");
    for (const auto& input : {std::string("1$"), std::string("1:02%")}) {
        for (const auto& language : {en, ru}) {
            const auto result = frontend.process(input, language);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
    }
    {
        const auto result = frontend.process("1 рублейfoo", ru);
        CHECK(result.normalized_text == "1 рублейfoo");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == std::string("1 рублейfoo").size());
    }
    for (const auto& input : {std::string("1 kgfoo"),
                              std::string("1 рублейfoo"),
                              std::string("1 dollarfoo"),
                              std::string("1 руб.foo")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    for (const auto& input :
         {std::string("1 руб.foo"), std::string("1,2 руб.foo"), std::string("1,2 кгfoo")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    for (const auto& input :
         {std::string("-$1"), std::string("+$1"), std::string("\xE2\x82\xAC-1")}) {
        for (const auto& language : {en, ru}) {
            const auto result = frontend.process(input, language);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
    }
    {
        const std::string euro = "\xe2\x82\xac"
                                 "1";
        const auto result = frontend.process(euro, en);
        CHECK(result.normalized_text == euro);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == euro.size());
    }
    {
        auto no_cleanup = en;
        no_cleanup.cleanup_spacing = false;
        const std::string input = "C++17,234–123";
        const auto result = frontend.process(input, no_cleanup);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == 5);
        CHECK(result.warnings.front().length == input.size() - 5);
    }
    CHECK(frontend.process("-1 kg", en).normalized_text == "minus one kilogram");
    CHECK(frontend.process("2 meter", en).normalized_text == "two meters");
    CHECK(frontend.process("1 MB 2 MB 1 GB 2 GB", en).normalized_text ==
          "one megabyte two megabytes one gigabyte two gigabytes");
    CHECK(frontend.process("$12.345", en).normalized_text == "$12.345");
    CHECK(frontend.process("1 234 567", en).normalized_text ==
          "one million two hundred thirty four thousand five hundred sixty seven");
    {
        const auto invalid_time = frontend.process("24:00 99:99", en);
        CHECK(invalid_time.normalized_text == "24:00 99:99");
        CHECK(invalid_time.warnings.size() == 2);
    }
    CHECK(frontend.process("There are 12 GPUs.", en).normalized_text == "There are twelve GPUs.");
    CHECK(frontend.process("тест42 тест42% тест12:34", ru).normalized_text ==
          "тест42 тест42% тест12:34");
    CHECK(frontend.process("слово1.2.2026", ru).normalized_text == "слово1.2.2026");
    CHECK(frontend.process("abc2 kg", en).normalized_text == "abc2 kg");
    const auto embedded_grouped = frontend.process("abc1 234", en);
    CHECK(embedded_grouped.normalized_text == "abc1 234");
    CHECK(!embedded_grouped.warnings.empty());
    CHECK(embedded_grouped.warnings.front().code == WarningCode::UnresolvedNumber);
    CHECK(frontend.process("тест1 234", ru).normalized_text == "тест1 234");
    const auto numeric_identifiers = frontend.process("12-34 123-456 555-1234 12/34", en);
    CHECK(numeric_identifiers.normalized_text == "12-34 123-456 555-1234 12/34");
    CHECK(!numeric_identifiers.warnings.empty());
    CHECK(numeric_identifiers.warnings.front().code == WarningCode::UnresolvedNumber);
    CHECK(frontend.process("abc1 23", en).normalized_text == "abc1 23");
    CHECK(frontend.process("abc1 234 56", en).normalized_text == "abc1 234 56");
    CHECK(frontend.process("1.2+3 1.2=3 abc/123 123+abc", en).normalized_text ==
          "1.2+3 1.2=3 abc/123 123+abc");
    CHECK(frontend.process("+7 999 123-45-67", en).normalized_text == "+7 999 123-45-67");
    CHECK(frontend.process("1,2кг", ru).normalized_text == "1,2кг");
    CHECK(frontend.process("1.2%word", en).normalized_text == "1.2%word");
    CHECK(frontend.process("01.02.2026г.", ru).normalized_text == "01.02.2026г.");
    const std::string decomposed_word = "e\xcc\x81"
                                        "42";
    CHECK(frontend.process(decomposed_word, en).normalized_text == decomposed_word);
    CHECK(frontend.process("x$12 RTX-4090 C++17 V2.1.0 #123", en).normalized_text ==
          "x$12 RTX-4090 C++17 V2.1.0 #123");
    for (const auto& technical_text : {std::string("https://example.com/1e3"),
                                       std::string("https://example.com/1e3?x=2"),
                                       std::string("user1e3@example.com"),
                                       std::string("RTX-1e3"),
                                       std::string("C++1e3")}) {
        const auto technical_result = frontend.process(technical_text, en);
        CHECK(technical_result.normalized_text == technical_text);
        CHECK(technical_result.warnings.empty());
    }
    const auto check_technical_numeric_tail = [&](const std::string& input, std::size_t offset) {
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(input, options);
            if (result.normalized_text != input || result.warnings.size() != 1 ||
                result.warnings.front().code != WarningCode::UnresolvedNumber ||
                result.warnings.front().offset != offset ||
                result.warnings.front().length != input.size() - offset)
                return false;
        }
        return true;
    };
    const auto check_warning_invariant = [&](const std::string& input,
                                             const TextFrontendOptions& options) {
        const auto result = frontend.process(input, options);
        bool valid = true;
        for (std::size_t left = 0; left < result.warnings.size(); ++left) {
            valid = valid &&
                    result.warnings[left].offset + result.warnings[left].length <= input.size();
            for (std::size_t right = left + 1; right < result.warnings.size(); ++right) {
                const auto& first = result.warnings[left];
                const auto& second = result.warnings[right];
                valid = valid && (first.offset + first.length <= second.offset ||
                                  second.offset + second.length <= first.offset);
            }
        }
        return valid;
    };
    CHECK(check_technical_numeric_tail("x$+1,234%", 1));
    CHECK(check_technical_numeric_tail("x$-1,234%", 1));
    CHECK(check_technical_numeric_tail("x+1,234%", 1));
    CHECK(check_technical_numeric_tail("x-1,234%", 1));
    CHECK(check_technical_numeric_tail("#1,234%", 0));
    CHECK(check_technical_numeric_tail("C++17,234%", 5));
    CHECK(check_technical_numeric_tail("RTX-4090,234%", 3));
    CHECK(check_technical_numeric_tail("V2.1.0,234%", 6));
    CHECK(check_technical_numeric_tail("a C++17,234%", 7));
    CHECK(check_technical_numeric_tail("HTTP/2,234%", 6));
    CHECK(check_technical_numeric_tail("x$+1,234 %", 1));
    CHECK(check_technical_numeric_tail("C++17,23%", 5));
    CHECK(check_technical_numeric_tail("x$+1,23%", 1));
    CHECK(check_technical_numeric_tail("C++17,1234%", 5));
    CHECK(check_technical_numeric_tail("C++17,12,345%", 5));
    CHECK(check_technical_numeric_tail("C++17,23%1:02", 5));
    CHECK(check_technical_numeric_tail("x$+1,23%1:02", 1));
    CHECK(check_technical_numeric_tail("C++17,23% 456", 5));
    CHECK(check_technical_numeric_tail("C++17,23 456%", 5));
    CHECK(check_technical_numeric_tail("x$+1,23 456%", 1));
    CHECK(check_technical_numeric_tail("C++17,234..5%", 5));
    CHECK(check_technical_numeric_tail("C++17,234...5%", 5));
    CHECK(check_technical_numeric_tail("C++17,234.0..5%", 5));
    CHECK(check_technical_numeric_tail("C++17,%1%", 5));
    CHECK(check_technical_numeric_tail("C++17, %1:02%", 5));
    CHECK(check_technical_numeric_tail("C++17,23% 456/789", 5));
    CHECK(check_technical_numeric_tail("C++17,23% 456 789%", 5));
    CHECK(check_technical_numeric_tail("C++17,23% + 456", 5));
    CHECK(check_technical_numeric_tail("C++17,234:567%", 5));
    CHECK(check_technical_numeric_tail("C++17,234/567%", 5));
    CHECK(check_technical_numeric_tail("C++17,23% 456*789", 5));
    CHECK(check_technical_numeric_tail("C++17,23%1/2", 5));
    CHECK(check_technical_numeric_tail("C++17,23%12/34", 5));
    CHECK(check_technical_numeric_tail("C++17,23%123 456", 5));
    CHECK(check_technical_numeric_tail("RTX-4090%123", 3));
    CHECK(check_technical_numeric_tail("C++17%123", 5));
    CHECK(check_technical_numeric_tail("#123%456", 0));
    CHECK(check_technical_numeric_tail("x+1%123", 1));
    CHECK(check_technical_numeric_tail(std::string("C++17,234") + "\xE2\x80\x93" + "123", 5));
    CHECK(check_technical_numeric_tail(std::string("x$+1,234") + "\xE2\x80\x93" + "123", 1));
    CHECK(check_technical_numeric_tail(std::string("HTTP/2,234") + "\xE2\x80\x94" + "123", 6));
    CHECK(check_technical_numeric_tail(std::string("V2.1.0,234") + "\xE2\x88\x92" + "123", 6));
    {
        const auto input = std::string("C++17,234") + "\xE2\x80\x93" + "word";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(input, options);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().offset == 5);
            CHECK(result.warnings.front().length == 4);
        }
    }
    CHECK(check_technical_numeric_tail(std::string("C++17 ") + "\xE2\x80\x93" + " 123", 5));
    CHECK(check_technical_numeric_tail(std::string("#123 ") + "\xE2\x88\x92" + " 456", 0));
    CHECK(check_technical_numeric_tail("C++17%123word", 5));
    CHECK(check_technical_numeric_tail("C++17%123РєРёРІРѕ", 5));
    CHECK(check_technical_numeric_tail("RTX-4090/123", 3));
    CHECK(check_technical_numeric_tail("C++17=123", 5));
    CHECK(check_technical_numeric_tail("#123/456", 0));
    CHECK(check_technical_numeric_tail("x$- 1,234%123 456", 1));
    CHECK(check_technical_numeric_tail("V2.1.0-123", 6));
    CHECK(check_technical_numeric_tail("HTTP/2-123", 6));
    CHECK(check_technical_numeric_tail("#123-456", 0));
    CHECK(check_technical_numeric_tail("C++17–123", 5));
    CHECK(check_technical_numeric_tail("V2.1.0—123", 6));
    CHECK(check_technical_numeric_tail("#123−456", 0));
    {
        const std::string unicode_suffix = "C++17%123—word";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(unicode_suffix, options);
            CHECK(result.normalized_text == unicode_suffix);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().offset == 5);
            CHECK(result.warnings.front().length == 4);
        }
    }
    CHECK(check_technical_numeric_tail("$- 1,234%123 456", 0));
    {
        const std::string separated = "C++17,23% HTTP/2,12 345%";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(separated, options);
            CHECK(result.normalized_text == separated);
            CHECK(result.warnings.size() == 2);
            CHECK(result.warnings.front().offset == 5);
            CHECK(result.warnings.front().length == 4);
        }
    }
    for (const auto& input : {std::string("C++17,23% #1,234%"),
                              std::string("#1,23% #2,34%"),
                              std::string("C++17,23% 127.0.0.1,234%")}) {
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(input, options);
            CHECK(result.normalized_text == input);
            CHECK(!result.warnings.empty());
            CHECK(result.warnings.size() <= 2);
            CHECK(check_warning_invariant(input, options));
        }
    }
    {
        std::string long_grouped = "C++17";
        for (int index = 0; index < 7800; ++index)
            long_grouped += ",123";
        long_grouped += '%';
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(long_grouped, options);
            CHECK(result.normalized_text == long_grouped);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 5);
            CHECK(result.warnings.front().length == long_grouped.size() - 5);
        }
    }
    {
        const std::string technical_url = "https://example.com/C++17,234%25";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(technical_url, options);
            CHECK(result.normalized_text == technical_url);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == technical_url.find(','));
            CHECK(result.warnings.front().length == 7);
        }
    }
    CHECK(frontend.process("1.2.3 12:34:56 1,000.50 1.2.3%", en).normalized_text ==
          "1.2.3 12:34:56 one thousand point five zero 1.2.3%");
    {
        const auto punctuation = frontend.process("12.5%. 1:02.", en);
        CHECK(punctuation.normalized_text == "twelve point five percent. one hour two minutes.");
        CHECK(punctuation.warnings.empty());
    }
    {
        const auto punctuation = frontend.process("01.02.2026.", ru);
        CHECK(punctuation.normalized_text != "01.02.2026.");
        CHECK(!punctuation.normalized_text.empty());
        CHECK(punctuation.normalized_text.back() == '.');
        CHECK(punctuation.warnings.empty());
    }
    {
        const std::string embedded_numeric = "abc1,234 1,23%";
        const auto result = frontend.process(embedded_numeric, en);
        CHECK(result.normalized_text == embedded_numeric);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 3);
        CHECK(result.warnings.front().length == embedded_numeric.size() - 3);
    }
    for (const auto& input : {std::string("1,234.56!"),
                              std::string("$1,234.56!"),
                              std::string("1,234+abc"),
                              std::string("1,234.56%word"),
                              std::string("1.2!"),
                              std::string("1.2?"),
                              std::string("1.2)"),
                              std::string("12:34!"),
                              std::string("01.02.2026?")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
    }
    CHECK(frontend.process("1,234 руб.", ru).normalized_text == "1,234 руб.");
    for (const auto& input : {std::string("$1,234%"),
                              std::string("$1,234.56%"),
                              std::string("x$1,234%"),
                              std::string("$ 1,234%"),
                              std::string("$+1,234%")}) {
        const auto result = frontend.process(input, en);
        const auto numeric_offset = input.front() == 'x' ? 1u : 0u;
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == numeric_offset);
        CHECK(result.warnings.front().length == input.size() - numeric_offset);
    }
    for (const auto& input : {std::string("$ 1,234%"), std::string("$+1,234%")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    const std::string nbsp = "a\xc2\xa0"
                             "b\xe2\x80\xaf"
                             "c\x0b"
                             "d\x0c"
                             "e";
    CHECK(frontend.process(nbsp, en).normalized_text == "a b c d e");
    {
        const std::string nested_currency_url = "https://example.com/path/$1,234%25";
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(nested_currency_url, options);
            CHECK(result.normalized_text == nested_currency_url);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == nested_currency_url.find('$'));
            CHECK(result.warnings.front().length ==
                  nested_currency_url.size() - nested_currency_url.find('$'));
        }
    }
    for (const auto& input : {std::string("$1,234%1/2"),
                              std::string("$1,234%12/34"),
                              std::string("$1,234%123 456"),
                              std::string("$ 1,234%123 456"),
                              std::string("$+1,234%123 456"),
                              std::string("$+ 1,234%123 456")}) {
        for (const auto& options : {en, ru}) {
            const auto result = frontend.process(input, options);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
    }

    PronunciationDictionary dictionary;
    CHECK(dictionary.add_token("замок", "замок", 1));
    CHECK(dictionary.add_case_insensitive_token("Qwen", "квен"));
    CHECK(dictionary.add_phrase("New York", "Нью-Йорк"));
    CHECK(!dictionary.add_token("замок", "замок", 1));
    CHECK(dictionary.add_case_insensitive_token("Москва", "москва"));
    TextFrontendOptions dictionary_options;
    dictionary_options.language = Language::English;
    dictionary_options.dictionary = &dictionary;
    const auto dictionary_result = frontend.process("QWEN New York", dictionary_options);
    CHECK(dictionary_result.pronunciation_text == "квен Нью-Йорк");
    CHECK(dictionary_result.words.front().from_dictionary == true);
    CHECK(dictionary_result.dictionary_replacements.size() == 2);
    CHECK(dictionary_result.words[1].dictionary_replacement.has_value());
    CHECK(dictionary_result.words[2].dictionary_replacement.has_value());
    CHECK(frontend.process("QWEN.", dictionary_options).pronunciation_text == "квен.");
    CHECK(frontend.process("МОСКВА", dictionary_options).pronunciation_text == "москва");
    dictionary_options.language = Language::Russian;
    CHECK(frontend.process("замок", dictionary_options).words.front().stressed_vowel == 1);
    dictionary_options.stress_mode = StressMode::Disabled;
    {
        const auto disabled = frontend.process("замок", dictionary_options);
        CHECK(!disabled.words.front().stressed_vowel.has_value());
        CHECK(disabled.stress_decisions.empty());
    }
    dictionary_options.stress_mode = StressMode::DictionaryOnly;
    dictionary_options.resolve_stress = false;
    {
        const auto disabled = frontend.process("замок", dictionary_options);
        CHECK(!disabled.words.front().stressed_vowel.has_value());
        CHECK(disabled.stress_decisions.empty());
    }
    dictionary_options.resolve_stress = true;
    CHECK(frontend.process("New Yorkshire", dictionary_options).pronunciation_text ==
          "New Yorkshire");

    // Automatic initialism expansion is opt-in and deliberately allowlisted.
    TextFrontendOptions initialisms;
    initialisms.language = Language::Russian;
    initialisms.normalize = false;
    initialisms.expand_initialisms = true;
    const auto initialism_result = frontend.process("ВК ООО РФ МГУ", initialisms);
    CHECK(initialism_result.pronunciation_text == "вэ ка о о о эр эф эм гэ у");
    CHECK(initialism_result.automatic_rewrites.size() == 4);
    CHECK(initialism_result.words.size() == 4);
    CHECK(initialism_result.words[0].surface == "ВК");
    CHECK(initialism_result.words[0].pronunciation == "вэ ка");
    CHECK(initialism_result.words[0].from_automatic_rewrite);
    TextFrontendOptions english_initialisms = initialisms;
    english_initialisms.language = Language::English;
    CHECK(frontend.process("ВК ООО", english_initialisms).pronunciation_text == "ВК ООО");
    CHECK(frontend.process("ФСБ МФЦ ИП", initialisms).pronunciation_text ==
          "эф эс бэ эм эф цэ и пэ");
    CHECK(frontend.process("ВК вк Вк", initialisms).pronunciation_text == "вэ ка вк Вк");
    CHECK(frontend.process("НАТО МИД ЗАГС", initialisms).pronunciation_text == "НАТО МИД ЗАГС");
    CHECK(frontend.process("ВК🙂 🙂ВК ООО❤️", initialisms).pronunciation_text ==
          "вэ ка🙂 🙂вэ ка о о о❤️");

    PronunciationDictionary rewrite_dictionary;
    CHECK(rewrite_dictionary.add_token("ВК", "явный override"));
    CHECK(rewrite_dictionary.add_token("ООО", "word"));
    CHECK(rewrite_dictionary.add_token("Visual", "word"));
    CHECK(rewrite_dictionary.add_phrase("Visual Studio", "phrase"));
    CHECK(rewrite_dictionary.add_phrase("ООО Ромашка", "phrase acronym"));
    TextFrontendOptions rewrite_options = initialisms;
    rewrite_options.language = Language::English;
    rewrite_options.dictionary = &rewrite_dictionary;
    const auto rewrite_result = frontend.process("ВК Visual Studio", rewrite_options);
    CHECK(rewrite_result.pronunciation_text == "явный override phrase");
    CHECK(rewrite_result.automatic_rewrites.empty());
    CHECK(frontend.process("ООО Ромашка", rewrite_options).pronunciation_text == "phrase acronym");

    PronunciationDictionary phrase_only_dictionary;
    CHECK(phrase_only_dictionary.add_phrase("Visual Studio", "phrase"));
    CHECK(phrase_only_dictionary.add_phrase("ООО Ромашка", "phrase acronym"));
    TextFrontendOptions phrase_options = rewrite_options;
    phrase_options.dictionary = &phrase_only_dictionary;
    phrase_options.expand_initialisms = false;
    const auto opaque_phrase = frontend.process("Visual🙂Studio", phrase_options);
    CHECK(opaque_phrase.pronunciation_text == "Visual🙂Studio");
    CHECK(frontend.process("Visual—Studio", phrase_options).pronunciation_text == "Visual—Studio");
    CHECK(frontend.process("Visual / Studio", phrase_options).pronunciation_text ==
          "Visual / Studio");
    CHECK(frontend.process("ООО🙂Ромашка", phrase_options).pronunciation_text == "ООО🙂Ромашка");
    CHECK(frontend.process("ООО ❤️ Ромашка", phrase_options).pronunciation_text == "ООО ❤️ Ромашка");

    PronunciationDictionary overlapping_dictionary;
    CHECK(overlapping_dictionary.add_token("New", "single-token"));
    CHECK(overlapping_dictionary.add_phrase("New York", "phrase-pronunciation"));
    dictionary_options.dictionary = &overlapping_dictionary;
    const auto overlapping_result = frontend.process("New York", dictionary_options);
    CHECK(overlapping_result.pronunciation_text == "phrase-pronunciation");
    CHECK(overlapping_result.words.size() == 2);
    CHECK(overlapping_result.words[0].pronunciation == "phrase-pronunciation");
    CHECK(overlapping_result.words[1].pronunciation.empty());
    dictionary_options.dictionary = &dictionary;
    std::vector<TextWarning> dictionary_warnings;
    PronunciationDictionary loaded;
    CHECK(loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/dictionary.json",
                           &dictionary_warnings));
    CHECK(dictionary_warnings.empty());
    CHECK(loaded.find_token("QWEN") != nullptr);
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/malformed_dictionary.json",
                            &dictionary_warnings));
    CHECK(loaded.find_token("broken") == nullptr);
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/malformed_unknown.json",
                            &dictionary_warnings));
    CHECK(
        !loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/unknown_field.json",
                          &dictionary_warnings));
    CHECK(
        !loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/duplicate_key.json",
                          &dictionary_warnings));
    CHECK(!loaded.load_file(std::string(TTS_FRONT_SOURCE_DIR) +
                                "/tests/fixtures/case_insensitive_duplicates.json",
                            &dictionary_warnings));
    const std::string control_json =
        std::string(TTS_FRONT_SOURCE_DIR) + "/tests/fixtures/control_char_runtime.json";
    {
        std::ofstream file(control_json, std::ios::binary);
        file << "[{\"pattern\":\"bad";
        file.put('\x01');
        file << "\",\"pronunciation\":\"ok\"}]";
    }
    CHECK(!loaded.load_file(control_json, &dictionary_warnings));
    std::remove(control_json.c_str());
    CHECK(!loaded.add_case_insensitive_token("QWEN", "other"));
    CHECK(!loaded.add_token(std::string("\xc0\xaf", 2), "bad"));

    TextFrontendOptions automatic;
    automatic.language = Language::Russian;
    automatic.stress_mode = StressMode::Automatic;
    const auto automatic_result = frontend.process("тест", automatic);
    CHECK(automatic_result.warnings.size() == 1);
    CHECK(automatic_result.warnings.front().code == WarningCode::AutomaticStressUnavailable);
    CHECK(automatic_result.warnings.front().offset == 0);
    CHECK(automatic_result.warnings.front().length == 0);
    automatic.resolve_stress = false;
    CHECK(frontend.process("тест", automatic).warnings.empty());
    TextFrontendOptions unsupported;
    unsupported.language = static_cast<Language>(999);
    const auto unsupported_result = frontend.process("test", unsupported);
    CHECK(unsupported_result.warnings.size() == 1);
    CHECK(unsupported_result.warnings.front().code == WarningCode::UnsupportedLanguage);
    CHECK(unsupported_result.has_uncertainty());
    CHECK(unsupported_result.warnings.front().offset == 0);
    CHECK(unsupported_result.warnings.front().length == 0);
    TextFrontendOptions no_cleanup;
    no_cleanup.language = Language::English;
    no_cleanup.cleanup_spacing = false;
    CHECK(frontend.process("a  b", no_cleanup).normalized_text == "a  b");
    CHECK(frontend.process("café 😊", en).normalized_text == "café 😊");
    const auto mixed = frontend.process("Привет API", TextFrontendOptions{});
    CHECK(mixed.has_uncertainty());
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result =
            frontend.process("У меня 2 GPU и 3 ядра. Использую OpenAI API.", segmented);
        CHECK(result.normalized_text == "У меня два GPU и три ядра. Использую OpenAI API.");
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::AmbiguousNormalization);
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        segmented.cleanup_spacing = false;
        const std::string malformed[] = {"Привет 2 kgfoo",
                                         "Привет «2 kg»",
                                         "Привет /2 kg",
                                         "Привет –2 kg",
                                         "Привет —2 kg",
                                         "Привет −2 kg",
                                         "Привет 1234567890 kg",
                                         "Привет -$1",
                                         "Привет +$1",
                                         "Привет +1.2 kg",
                                         "Привет +1*2",
                                         "Привет +$1,234.56",
                                         "Привет −1:02"};
        for (const auto& input : malformed) {
            const auto result = frontend.process(input, segmented);
            CHECK(result.normalized_text == input);
            std::size_t unresolved = 0;
            for (const auto& warning : result.warnings)
                unresolved += warning.code == WarningCode::UnresolvedNumber ? 1 : 0;
            CHECK(unresolved == 1);
            const auto warning = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& item) {
                    return item.code == WarningCode::UnresolvedNumber;
                });
            CHECK(warning != result.warnings.end());
            auto expected_begin = input.find("2 kg");
            if (expected_begin == std::string::npos)
                expected_begin = input.find("1234567890");
            if (const auto connector = input.find("/2"); connector != std::string::npos)
                expected_begin = connector;
            if (const auto connector = input.find("–2"); connector != std::string::npos)
                expected_begin = connector;
            if (const auto connector = input.find("—2"); connector != std::string::npos)
                expected_begin = connector;
            if (const auto connector = input.find("−2"); connector != std::string::npos)
                expected_begin = connector;
            if (const auto sign = input.find("-$1"); sign != std::string::npos)
                expected_begin = sign;
            if (const auto sign = input.find("+$1"); sign != std::string::npos)
                expected_begin = sign;
            if (const auto sign = input.find("+1.2"); sign != std::string::npos)
                expected_begin = sign;
            if (const auto sign = input.find("+1*2"); sign != std::string::npos)
                expected_begin = sign;
            if (const auto sign = input.find("+$1"); sign != std::string::npos)
                expected_begin = sign;
            if (const auto sign = input.find("−1"); sign != std::string::npos)
                expected_begin = sign;
            CHECK(warning->offset == expected_begin);
        }
        const auto negative_zero = frontend.process("У меня -0 kg", segmented);
        CHECK(negative_zero.normalized_text == "У меня minus zero kilograms");
        const auto negative_zero_decimal = frontend.process("У меня -0.0 kg", segmented);
        CHECK(negative_zero_decimal.normalized_text == "У меня minus zero point zero kilograms");
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        segmented.cleanup_spacing = false;
        for (const auto& input : {std::string("Привет 99:99 2 kgfoo"),
                                  std::string("Привет 1$ 2 kgfoo"),
                                  std::string("Привет 1,234.56! 2 kgfoo")}) {
            const auto result = frontend.process(input, segmented);
            const auto candidate = input.find("2 kgfoo");
            CHECK(candidate != std::string::npos);
            CHECK(result.normalized_text == input);
            const auto warning = std::find_if(
                result.warnings.begin(),
                result.warnings.end(),
                [candidate](const TextWarning& item) {
                    return item.code == WarningCode::UnresolvedNumber && item.offset == candidate;
                });
            CHECK(warning != result.warnings.end());
            CHECK(warning->length == input.size() - candidate);
        }
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        segmented.cleanup_spacing = false;
        for (const auto& input : {std::string("Hello 2 кг-3"),
                                  std::string("Hello 2 кг—3"),
                                  std::string("Hello 2 руб./3")}) {
            const auto result = frontend.process(input, segmented);
            const auto candidate = input.find("2 ");
            CHECK(result.normalized_text == input);
            const auto warning = std::find_if(
                result.warnings.begin(),
                result.warnings.end(),
                [candidate](const TextWarning& item) {
                    return item.code == WarningCode::UnresolvedNumber && item.offset == candidate;
                });
            CHECK(warning != result.warnings.end());
            CHECK(warning->length == input.size() - candidate);
        }
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        segmented.cleanup_spacing = false;
        for (const auto& input : {std::string("Привет 2 kg-3"),
                                  std::string("Привет 2 kg+3"),
                                  std::string("Привет 2 kg—3"),
                                  std::string("Привет 2 kg%3"),
                                  std::string("Привет x $1/2")}) {
            const auto result = frontend.process(input, segmented);
            const auto candidate =
                input.find(input.find("$1") != std::string::npos ? "$1" : "2 kg");
            CHECK(candidate != std::string::npos);
            CHECK(result.normalized_text == input);
            const auto warning = std::find_if(
                result.warnings.begin(),
                result.warnings.end(),
                [candidate](const TextWarning& item) {
                    return item.code == WarningCode::UnresolvedNumber && item.offset == candidate;
                });
            CHECK(warning != result.warnings.end());
            CHECK(warning->length == input.size() - candidate);
        }
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result =
            frontend.process("Видеокарта RTX 4090 работает на 1.5 GB RAM.", segmented);
        CHECK(result.normalized_text ==
              "Видеокарта RTX 4090 работает на one point five gigabytes RAM.");
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::AmbiguousNormalization);
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result = frontend.process("Use 2 кг", segmented);
        CHECK(result.normalized_text == "Use два килограмма");
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().code == WarningCode::AmbiguousNormalization);
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const std::array<std::pair<std::string, std::string>, 10> local_heads = {{
            {"OpenAI API processed 2 запроса.", "OpenAI API processed два запроса."},
            {"RTX-4090 completed 4 задачи.", "RTX-4090 completed четыре задачи."},
            {"HTTP/2 returned 5 ошибок.", "HTTP/2 returned пять ошибок."},
            {"Build 7 passed в CI. Проверка завершена.",
             "Build seven passed в CI. Проверка завершена."},
            {"Version V2.1.0 includes 3 исправления.", "Version V2.1.0 includes три исправления."},
            {"RTX-4090 завершил 4 tasks.", "RTX-4090 завершил four tasks."},
            {"HTTP/2 вернул 5 errors.", "HTTP/2 вернул five errors."},
            {"Отправьте 2 reports на dev@example.com.",
             "Отправьте two reports на dev@example.com."},
            {"Сборка 7 прошла в CI. Validation complete.",
             "Сборка семь прошла в CI. Validation complete."},
            {"Версия V2.1.0 содержит 3 fixes.", "Версия V2.1.0 содержит three fixes."},
        }};
        for (const auto& [input, expected] : local_heads) {
            const auto result = frontend.process(input, segmented);
            CHECK(result.normalized_text == expected);
            CHECK(result.pronunciation_text == expected);
            CHECK(std::any_of(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::AmbiguousNormalization;
                }));
        }
        for (const auto& input : {std::string("Привет hello 2 запроса#4"),
                                  std::string("Привет hello 2 запроса /4"),
                                  std::string("Привет hello 2 kgfoo")}) {
            const auto result = frontend.process(input, segmented);
            CHECK(result.normalized_text == input);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved != result.warnings.end());
            CHECK(unresolved->offset == input.find("2"));
            CHECK(unresolved->length == input.size() - input.find("2"));
        }
    }
    {
        const std::string mixed_invalid_input = "Привет hello 99:99";
        const auto mixed_invalid = frontend.process(mixed_invalid_input, TextFrontendOptions{});
        CHECK(mixed_invalid.warnings.size() == 2);
        CHECK(mixed_invalid.warnings[0].code == WarningCode::AmbiguousNormalization);
        CHECK(mixed_invalid.warnings[0].offset == 0);
        CHECK(mixed_invalid.warnings[0].length == mixed_invalid_input.size());
        CHECK(mixed_invalid.warnings[1].offset == std::string("Привет hello ").size());
        CHECK(mixed_invalid.warnings[1].length == 5);
    }
    CHECK(frontend.process("__TTS_PROTECTED_A__ RTX 4090", en).normalized_text ==
          "__TTS_PROTECTED_A__ RTX 4090");
    const std::string huge(80, '9');
    CHECK(frontend.process(huge + "% $" + huge + " " + huge + ".1 " + huge + " kg", en)
              .has_uncertainty());
    CHECK(frontend.process(huge + "% " + huge + ",1 " + huge + " кг", ru).has_uncertainty());
    CHECK(frontend.process("В 2042 г.", ru).normalized_text == "В две тысячи сорок втором году");
    CHECK(frontend.process("В 5001 г.", ru).normalized_text == "В пять тысяч первом году");
    CHECK(
        frontend.process("01.02.2011 01.02.2012 01.02.2019 01.02.2020 01.02.2042 01.02.5001", ru)
            .normalized_text ==
        "первое февраля две тысячи одиннадцатого года первое февраля две тысячи двенадцатого года "
        "первое февраля две тысячи девятнадцатого года первое февраля две тысячи двадцатого года "
        "первое февраля две тысячи сорок второго года первое февраля пять тысяч первого года");
    CHECK(frontend.process("01.02.2025", ru).normalized_text ==
          "первое февраля две тысячи двадцать пятого года");
    CHECK(frontend.process("31.12.1987", ru).normalized_text ==
          "тридцать первое декабря тысяча девятьсот восемьдесят седьмого года");
    CHECK(frontend.process("0,01 0,21 1,123", ru).normalized_text ==
          "ноль целых одна сотая ноль целых двадцать одна сотая одна целая сто двадцать три "
          "тысячных");
    CHECK(frontend.process("1,1234", ru).normalized_text == "1,1234");
    CHECK(frontend.process("7,5%", ru).normalized_text == "семь целых пять десятых процента");
    CHECK(frontend.process("1,2 руб.", ru).normalized_text == "один рубль двадцать копеек.");
    {
        TextFrontendOptions auto_ru = ru;
        auto_ru.language = Language::Auto;
        TextFrontendOptions segmented_ru = auto_ru;
        segmented_ru.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const std::array<TextFrontendOptions, 3> currency_modes = {ru, auto_ru, segmented_ru};
        for (const auto& options : currency_modes) {
            const auto result = frontend.process("Итоговая цена — 2,50 руб.", options);
            CHECK(result.normalized_text == "Итоговая цена — два рубля пятьдесят копеек.");
            CHECK(result.pronunciation_text == result.normalized_text);
            CHECK(result.warnings.empty());

            const auto sentence = frontend.process("Бухгалтерия учла 2,50 руб. сегодня.", options);
            CHECK(sentence.normalized_text ==
                  "Бухгалтерия учла два рубля пятьдесят копеек сегодня.");
            CHECK(sentence.pronunciation_text == sentence.normalized_text);
            CHECK(sentence.warnings.empty());
        }
        const std::array<std::pair<std::string, std::string>, 7> monetary_values = {{
            {"2,05 руб.", "два рубля пять копеек."},
            {"2,5 руб.", "два рубля пятьдесят копеек."},
            {"2,00 руб.", "два рубля."},
            {"0,50 руб.", "ноль рублей пятьдесят копеек."},
            {"-0,50 руб.", "минус ноль рублей пятьдесят копеек."},
            {"-2,50 руб.", "минус два рубля пятьдесят копеек."},
            {"-0,00 руб.", "минус ноль рублей."},
        }};
        for (const auto& [input, expected] : monetary_values) {
            for (const auto& options : currency_modes) {
                const auto result = frontend.process(input, options);
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == expected);
                CHECK(result.warnings.empty());
            }
        }
        for (const auto& form :
             {std::string("рубль"), std::string("рубля"), std::string("рублей")}) {
            for (const auto& options : currency_modes) {
                const auto result = frontend.process("2,50 " + form, options);
                CHECK(result.normalized_text == "два рубля пятьдесят копеек");
                CHECK(result.pronunciation_text == result.normalized_text);
                CHECK(result.warnings.empty());
            }
        }
        const std::array<std::string, 6> unsupported_currency_forms = {"+2,50 руб.",
                                                                       std::string("−2,50 руб."),
                                                                       "+0,50 руб.",
                                                                       "1 250,50 руб.",
                                                                       "+2,50 ₽",
                                                                       std::string("−2,50 ₽")};
        for (const auto& input : unsupported_currency_forms) {
            for (const auto& options : currency_modes) {
                const auto result = frontend.process(input, options);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.front().offset == 0);
                CHECK(result.warnings.front().length == input.size());
            }
        }
        const std::array<std::pair<std::string, std::size_t>, 13> unsupported_currency_boundaries =
            {{
                {"+2,50 руб", std::string("+2,50 руб").size()},
                {"+2,50 руб.!", std::string("+2,50 руб.").size()},
                {"+2,50 руб./2", std::string("+2,50 руб./2").size()},
                {"−2,50 руб./2", std::string("−2,50 руб./2").size()},
                {"+2,50 рубfoo", std::string("+2,50 рубfoo").size()},
                {"+2,50 рубежей", std::string("+2,50 рубежей").size()},
                {"+2,50 руб́", std::string("+2,50 руб́").size()},
                {"+2,50 руб́ежей", std::string("+2,50 руб́ежей").size()},
                {"2,500 руб...", std::string("2,500 руб...").size()},
                {"2,500 руб.!", std::string("2,500 руб.").size()},
                {"2,500 руб./2", std::string("2,500 руб./2").size()},
                {"−1 250,500 руб́", std::string("−1 250,500 руб́").size()},
                {"+1 250,500 ₽", std::string("+1 250,500 ₽").size()},
            }};
        for (const auto& [input, expected_span] : unsupported_currency_boundaries) {
            for (const auto& options : currency_modes) {
                const auto result = frontend.process(input, options);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                const auto unresolved = std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved != result.warnings.end());
                CHECK(std::count_if(result.warnings.begin(),
                                    result.warnings.end(),
                                    [](const TextWarning& warning) {
                                        return warning.code == WarningCode::UnresolvedNumber;
                                    }) == 1);
                CHECK(unresolved->offset == 0);
                CHECK(unresolved->length == expected_span);
            }
        }
        for (const auto& options : currency_modes) {
            const auto independent = frontend.process("+2,50 руб. 3", options);
            CHECK(independent.normalized_text == "+2,50 руб. три");
            CHECK(independent.warnings.size() == 1);
            CHECK(independent.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(independent.warnings.front().offset == 0);
            CHECK(independent.warnings.front().length == std::string("+2,50 руб.").size());
        }
        for (const auto& options : currency_modes) {
            for (const auto& input : {std::string("2,50 ₽"), std::string("₽2,50")}) {
                const auto unsupported_symbol = frontend.process(input, options);
                CHECK(unsupported_symbol.normalized_text == input);
                CHECK(unsupported_symbol.pronunciation_text == unsupported_symbol.normalized_text);
                CHECK(unsupported_symbol.warnings.size() == 1);
                CHECK(unsupported_symbol.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(unsupported_symbol.warnings.front().offset == 0);
                CHECK(unsupported_symbol.warnings.front().length ==
                      unsupported_symbol.original_text.size());
            }
        }
        for (const auto& input : {std::string("2.50 ₽"), std::string("₽2.50")}) {
            const auto unsupported_symbol = frontend.process(input, en);
            CHECK(unsupported_symbol.normalized_text == input);
            CHECK(unsupported_symbol.pronunciation_text == input);
            CHECK(unsupported_symbol.warnings.size() == 1);
            CHECK(unsupported_symbol.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(unsupported_symbol.warnings.front().offset == 0);
            CHECK(unsupported_symbol.warnings.front().length == input.size());
        }
        for (const auto& options : currency_modes) {
            const auto unsupported = frontend.process("2,500 руб.", options);
            CHECK(unsupported.normalized_text == "2,500 руб.");
            CHECK(unsupported.warnings.size() == 1);
            CHECK(unsupported.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(unsupported.warnings.front().offset == 0);
            CHECK(unsupported.warnings.front().length == unsupported.original_text.size());
            const auto grouped = frontend.process("1 250,50 руб.", options);
            CHECK(grouped.normalized_text == grouped.original_text);
            CHECK(grouped.warnings.size() == 1);
            CHECK(grouped.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(grouped.warnings.front().offset == 0);
            CHECK(grouped.warnings.front().length == grouped.original_text.size());
        }
        const std::array<std::pair<std::string, std::string>, 6> monetary_punctuation = {{
            {"2,50 руб.!", "два рубля пятьдесят копеек!"},
            {"2,50 руб.?", "два рубля пятьдесят копеек?"},
            {"2,50 руб.)", "два рубля пятьдесят копеек)"},
            {"2,50 руб.,", "два рубля пятьдесят копеек,"},
            {"2,50 руб.;", "два рубля пятьдесят копеек;"},
            {"2,50 руб...", "два рубля пятьдесят копеек..."},
        }};
        for (const auto& [input, expected] : monetary_punctuation) {
            for (const auto& options : currency_modes) {
                const auto result = frontend.process(input, options);
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == expected);
                CHECK(result.warnings.empty());
            }
        }
        for (const auto& options : currency_modes) {
            const auto connector = frontend.process("2,50 руб./2", options);
            CHECK(connector.normalized_text == connector.original_text);
            CHECK(connector.warnings.size() == 1);
            CHECK(connector.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(connector.warnings.front().offset == 0);
            CHECK(connector.warnings.front().length == connector.original_text.size());
        }
        const auto technical_currency = frontend.process("https://example.com/2,50руб.", ru);
        CHECK(technical_currency.normalized_text == technical_currency.original_text);
        CHECK(technical_currency.pronunciation_text == technical_currency.original_text);
        CHECK(technical_currency.warnings.empty());
        for (const auto& input : {std::string("https://example.com/2,500руб."),
                                  std::string("user2,500руб@example.com"),
                                  std::string("RTX-2,500руб.")}) {
            for (const auto& options : currency_modes) {
                const auto technical_neighbor = frontend.process(input, options);
                CHECK(technical_neighbor.normalized_text == input);
                CHECK(std::none_of(technical_neighbor.warnings.begin(),
                                   technical_neighbor.warnings.end(),
                                   [](const TextWarning& warning) {
                                       return warning.code == WarningCode::UnresolvedNumber;
                                   }));
            }
        }
        for (const auto& options : {auto_options, segmented_ru}) {
            const auto mixed = frontend.process("Используем 2,50 руб. today", options);
            CHECK(mixed.normalized_text == "Используем два рубля пятьдесят копеек today");
            CHECK(std::any_of(
                mixed.warnings.begin(), mixed.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::AmbiguousNormalization;
                }));
            const auto mixed_unsupported = frontend.process("Используем 2,500 руб. today", options);
            CHECK(mixed_unsupported.normalized_text == mixed_unsupported.original_text);
            const auto unresolved =
                std::find_if(mixed_unsupported.warnings.begin(),
                             mixed_unsupported.warnings.end(),
                             [](const TextWarning& warning) {
                                 return warning.code == WarningCode::UnresolvedNumber;
                             });
            CHECK(unresolved != mixed_unsupported.warnings.end());
            CHECK(unresolved->offset == mixed_unsupported.original_text.find("2"));
            CHECK(unresolved->length == std::string("2,500 руб.").size());
        }
        const auto mixed_end = frontend.process("Use 2,500 руб.", segmented_ru);
        CHECK(mixed_end.normalized_text == mixed_end.original_text);
        CHECK(std::any_of(
            mixed_end.warnings.begin(), mixed_end.warnings.end(), [](const TextWarning& warning) {
                return warning.code == WarningCode::AmbiguousNormalization;
            }));
        CHECK(std::any_of(
            mixed_end.warnings.begin(), mixed_end.warnings.end(), [](const TextWarning& warning) {
                return warning.code == WarningCode::UnresolvedNumber;
            }));
    }
    CHECK(frontend.process("1,2% -3,4%", ru).normalized_text ==
          "одна целая две десятых процента минус три целых четыре десятых процента");
    {
        const auto invalid_time = frontend.process("24:00 99:99", ru);
        CHECK(invalid_time.normalized_text == "24:00 99:99");
        CHECK(invalid_time.warnings.size() == 2);
        CHECK(invalid_time.warnings[0].offset == 0);
        CHECK(invalid_time.warnings[0].length == 5);
        CHECK(invalid_time.warnings[1].offset == 6);
        CHECK(invalid_time.warnings[1].length == 5);
    }
    {
        const auto transformed = frontend.process("1% 99:99", en);
        CHECK(transformed.normalized_text == "one percent 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 3);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("RTX 4090 99:99", en);
        CHECK(transformed.normalized_text == "RTX 4090 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 9);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("a   99:99", en);
        CHECK(transformed.normalized_text == "a 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 4);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("1 234 99:99", en);
        CHECK(transformed.normalized_text == "one thousand two hundred thirty four 99:99");
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == 6);
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("тест 99:99", en);
        CHECK(transformed.warnings.size() == 1);
        CHECK(transformed.warnings.front().offset == std::string("тест ").size());
        CHECK(transformed.warnings.front().length == 5);
    }
    {
        const auto transformed = frontend.process("99:99 24:99", en);
        CHECK(transformed.warnings.size() == 2);
        CHECK(transformed.warnings[0].offset == 0);
        CHECK(transformed.warnings[0].length == 5);
        CHECK(transformed.warnings[1].offset == 6);
        CHECK(transformed.warnings[1].length == 5);
    }
    {
        const auto protected_invalid = frontend.process("https://example.com 99:99", en);
        CHECK(protected_invalid.normalized_text == "https://example.com 99:99");
        CHECK(protected_invalid.warnings.size() == 1);
        CHECK(protected_invalid.warnings.front().offset ==
              protected_invalid.original_text.find("99:99"));
        CHECK(protected_invalid.warnings.front().length == 5);
    }
    {
        const std::string protected_duplicate = "https://example.com/99:99 99:99";
        const auto result = frontend.process(protected_duplicate, en);
        CHECK(result.normalized_text == protected_duplicate);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == result.original_text.rfind("99:99"));
        CHECK(result.warnings.front().length == 5);
    }
    {
        const auto multi_transform = frontend.process("1% 1.5 99:99", en);
        CHECK(multi_transform.normalized_text == "one percent one point five 99:99");
        CHECK(multi_transform.warnings.size() == 1);
        CHECK(multi_transform.warnings.front().offset ==
              multi_transform.original_text.find("99:99"));
        CHECK(multi_transform.warnings.front().length == 5);
    }
    {
        const std::string grouped_before_invalid = "1   234 99:99";
        const auto result = frontend.process(grouped_before_invalid, en);
        CHECK(result.normalized_text == "one thousand two hundred thirty four 99:99");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == result.original_text.rfind("99:99"));
        CHECK(result.warnings.front().length == 5);
    }
    {
        const std::string oversized_measurement = std::string(21, '9') + "   kg";
        const auto result = frontend.process(oversized_measurement, en);
        CHECK(result.normalized_text == std::string(21, '9') + " kg");
        CHECK(!result.warnings.empty());
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == oversized_measurement.size());
    }
    {
        const auto unsupported_decimal = frontend.process("$12.345", en);
        CHECK(unsupported_decimal.normalized_text == "$12.345");
        CHECK(unsupported_decimal.warnings.size() == 1);
        CHECK(unsupported_decimal.warnings.front().offset == 0);
        CHECK(unsupported_decimal.warnings.front().length == 7);
    }
    {
        const auto preserved_with_valid_neighbor = frontend.process("$12.345 2 kg", en);
        CHECK(preserved_with_valid_neighbor.normalized_text == "$12.345 two kilograms");
        CHECK(preserved_with_valid_neighbor.warnings.size() == 1);
        CHECK(preserved_with_valid_neighbor.warnings.front().offset == 0);
        CHECK(preserved_with_valid_neighbor.warnings.front().length == 7);
    }
    {
        const auto malformed_ordinal = frontend.process("11st 2nd", en);
        CHECK(malformed_ordinal.normalized_text == "11st second");
        CHECK(malformed_ordinal.warnings.size() == 1);
        CHECK(malformed_ordinal.warnings.front().offset == 0);
        CHECK(malformed_ordinal.warnings.front().length == 4);
    }
    {
        const auto signed_ordinal = frontend.process("-1st 2nd", en);
        CHECK(signed_ordinal.normalized_text == "-1st second");
        CHECK(signed_ordinal.warnings.size() == 1);
        CHECK(signed_ordinal.warnings.front().offset == 0);
        CHECK(signed_ordinal.warnings.front().length == 4);
    }
    {
        for (const auto& options : {en, ru, auto_options}) {
            const auto signed_time = frontend.process("-1:02", options);
            CHECK(signed_time.normalized_text == "-1:02");
            CHECK(signed_time.warnings.size() == 1);
            CHECK(signed_time.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(signed_time.warnings.front().offset == 0);
            CHECK(signed_time.warnings.front().length == 5);

            for (const auto& input :
                 {std::string("+1.2%"),      std::string("+1.2 kg"),    std::string("+1:02"),
                  std::string("+1,2%"),      std::string("+1*2"),       std::string("+1&2"),
                  std::string("+1^2"),       std::string("+1–2"),       std::string("+1—2"),
                  std::string("+1−2"),       std::string("+1=2"),       std::string("+1e+3"),
                  std::string("+1#2"),       std::string("+$1,234.56"), std::string("-$1,234"),
                  std::string("€-1,234"),    std::string("-€1,234.56"), std::string("-£1,234.56"),
                  std::string("−€1,234.56"), std::string("−1:02")}) {
                const auto signed_numeric = frontend.process(input, options);
                CHECK(signed_numeric.normalized_text == input);
                CHECK(signed_numeric.warnings.size() == 1);
                CHECK(signed_numeric.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(signed_numeric.warnings.front().offset == 0);
                CHECK(signed_numeric.warnings.front().length == input.size());
            }
            for (const auto& input :
                 {std::string("−1 kg"),   std::string("−1.2 kg"),  std::string("−1,2 кг"),
                  std::string("−1word"),  std::string("+1e"),      std::string("+1e+"),
                  std::string("+1e+x"),   std::string("+1e++x"),   std::string("+1e--x"),
                  std::string("+1e+%"),   std::string("+1e+3 kg"), std::string("+1e+3/4"),
                  std::string("+1e+3#4"), std::string("+1e+3$4"),  std::string("−1e"),
                  std::string("−1e+"),    std::string("−1e+x"),    std::string("−1e−x"),
                  std::string("−1e+3#4"), std::string("−1e+3$4"),  std::string("-1e+3#4"),
                  std::string("+1e34#4")}) {
                const auto malformed_signed = frontend.process(input, options);
                CHECK(malformed_signed.normalized_text == input);
                CHECK(malformed_signed.warnings.size() == 1);
                CHECK(malformed_signed.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(malformed_signed.warnings.front().offset == 0);
                CHECK(malformed_signed.warnings.front().length == input.size());
            }
            for (const auto& input : {std::string("+1e–3"),
                                      std::string("+1e—3"),
                                      std::string("−1e–3"),
                                      std::string("−1e—3")}) {
                const auto malformed_dash = frontend.process(input, options);
                CHECK(malformed_dash.normalized_text == input);
                CHECK(malformed_dash.warnings.size() == 1);
                CHECK(malformed_dash.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(malformed_dash.warnings.front().offset == 0);
                CHECK(malformed_dash.warnings.front().length == input.size());
            }
            for (const auto& input : {std::string("+1e") + "\xE2\x80\x93" + "3#4",
                                      std::string("+1e") + "\xE2\x80\x94" + "3$4",
                                      std::string("\xE2\x88\x92"
                                                  "1e") +
                                          "\xE2\x80\x93" + "3#4",
                                      std::string("\xE2\x88\x92"
                                                  "1e") +
                                          "\xE2\x80\x94" + "3$4"}) {
                const auto malformed_dash_suffix = frontend.process(input, options);
                CHECK(malformed_dash_suffix.normalized_text == input);
                CHECK(malformed_dash_suffix.warnings.size() == 1);
                CHECK(malformed_dash_suffix.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(malformed_dash_suffix.warnings.front().offset == 0);
                CHECK(malformed_dash_suffix.warnings.front().length == input.size());
            }
            const std::vector<std::string> prefixes = {"", "abc"};
            const std::vector<std::string> initial_signs = {"", "+", "-", "\xE2\x88\x92"};
            const std::vector<std::string> exponent_connectors = {
                "+", "-", "\xE2\x80\x93", "\xE2\x80\x94"};
            const std::vector<std::string> exponent_lengths = {"3", "34", "3456"};
            const std::vector<std::string> suffixes = {
                "word", "#4", "$4", "%", "%word", "/word", " kg"};
            for (const auto& prefix : prefixes) {
                for (const auto& initial_sign : initial_signs) {
                    for (const auto& exponent_connector : exponent_connectors) {
                        for (const auto& exponent_digits : exponent_lengths) {
                            for (const auto& suffix : suffixes) {
                                const auto input = prefix + initial_sign + "1e" +
                                                   exponent_connector + exponent_digits + suffix;
                                const auto matrix_case = frontend.process(input, options);
                                CHECK(matrix_case.normalized_text == input);
                                CHECK(matrix_case.warnings.size() == 1);
                                CHECK(matrix_case.warnings.front().code ==
                                      WarningCode::UnresolvedNumber);
                                CHECK(matrix_case.warnings.front().offset == prefix.size());
                                CHECK(matrix_case.warnings.front().length ==
                                      input.size() - prefix.size());
                            }
                        }
                    }
                }
            }
        }
        const auto phone = frontend.process("+7 999 123 45 67", en);
        CHECK(phone.normalized_text == "+7 999 123 45 67");
        CHECK(phone.warnings.size() == 1);
        CHECK(phone.warnings.front().offset == 0);
        CHECK(phone.warnings.front().length == phone.original_text.size());
        const auto negative_zero_time = frontend.process("-0:00", en);
        CHECK(negative_zero_time.normalized_text == "-0:00");
        CHECK(negative_zero_time.warnings.size() == 1);
        CHECK(negative_zero_time.warnings.front().length == 5);
        const auto negative_zero_ordinal = frontend.process("-0th", en);
        CHECK(negative_zero_ordinal.normalized_text == "-0th");
        CHECK(negative_zero_ordinal.warnings.size() == 1);
        CHECK(negative_zero_ordinal.warnings.front().offset == 0);
        CHECK(negative_zero_ordinal.warnings.front().length == 4);
    }
    {
        const std::string oversized_ordinal = "999999999999999999999th";
        const auto result = frontend.process(oversized_ordinal, en);
        CHECK(result.normalized_text == oversized_ordinal);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == oversized_ordinal.size());
    }
    {
        const std::string huge_number(80, '9');
        const auto too_large = frontend.process(huge_number, en);
        CHECK(too_large.normalized_text == huge_number);
        CHECK(too_large.warnings.size() == 1);
        CHECK(too_large.warnings.front().offset == 0);
        CHECK(too_large.warnings.front().length == huge_number.size());
    }
    CHECK(frontend.process("99.99.2026", ru).normalized_text == "99.99.2026");
    {
        const auto malformed_date = frontend.process("99.99.2026", ru);
        CHECK(malformed_date.normalized_text == "99.99.2026");
        CHECK(malformed_date.warnings.size() == 1);
        CHECK(malformed_date.warnings.front().offset == 0);
        CHECK(malformed_date.warnings.front().length == std::string("99.99.2026").size());
    }
    {
        const auto invalid_english_date = frontend.process("February 31, 2026.", en);
        CHECK(invalid_english_date.normalized_text == "February 31, 2026.");
        CHECK(invalid_english_date.warnings.size() == 1);
        CHECK(invalid_english_date.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(invalid_english_date.warnings.front().offset == 0);
        CHECK(invalid_english_date.warnings.front().length ==
              std::string("February 31, 2026").size());
    }
    {
        // Date cores are admitted atomically before formatting.  Every
        // unsupported continuation must preserve the complete UTF-8 source
        // span, including a spaced percent continuation.
        for (const auto& input : {std::string("February 29, 2024%foo"),
                                  std::string("February 29, 2024+4"),
                                  std::string("February 29, 2024% 4"),
                                  std::string("February 29, 2024 %foo")}) {
            const auto result = frontend.process(input, en);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
    }
    {
        // Unit/currency formatters must not rewrite a supported core when an
        // unsupported connector tail follows it.
        for (const auto& [input, options] : {std::pair{std::string("2 kg#3"), en},
                                             std::pair{std::string("2 руб.$3"), ru},
                                             std::pair{std::string("2 кг#3"), ru},
                                             std::pair{std::string("2 kg $3"), en},
                                             std::pair{std::string("2 GB #3"), en},
                                             std::pair{std::string("2 кг #3"), ru},
                                             std::pair{std::string("2 руб. $3"), ru},
                                             std::pair{std::string("2 кг % 3"), ru},
                                             std::pair{std::string("2 kg /3"), en},
                                             std::pair{std::string("2 kg+3"), en},
                                             std::pair{std::string("2 kg−3"), en}}) {
            const auto result = frontend.process(input, options);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
        TextFrontendOptions segmented = auto_options;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        for (const auto& input : {std::string("2 kg $3"), std::string("Привет 2 kg #3")}) {
            const auto result =
                frontend.process(input, input.rfind("Привет", 0) == 0 ? segmented : auto_options);
            const auto begin = input.find("2");
            CHECK(result.normalized_text == input);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved != result.warnings.end());
            CHECK(unresolved->offset == begin);
            CHECK(unresolved->length == input.size() - begin);
        }
    }
    {
        const std::vector<std::string> unsupported_currency_tails = {
            "$1 #2",
            "$1 /2",
            "$1 %2",
            "$1,234 #2",
            "$1,234 /2",
            "$1,234 %2",
            "-$1 #2",
            "-$1 /2",
            "-$1 %2",
            "+$1 #2",
            "+$1 /2",
            "+$1 %2",
            std::string("\xE2\x88\x92") + "$1 #2",
            std::string("\xE2\x88\x92") + "$1,234 /2"};
        for (const auto& input : unsupported_currency_tails) {
            const auto result = frontend.process(input, en);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
        CHECK(frontend.process("$1 2", en).normalized_text == "one dollar two");
        CHECK(frontend.process("$1 $2", en).normalized_text == "one dollar two dollars");
    }
    {
        const std::vector<std::string> currency_prefixes = {
            "$ 1",
            "$+1",
            "$-1",
            std::string("$") + "\xE2\x88\x92" + "1",
            "+$1",
            "-$1",
            std::string("\xE2\x88\x92") + "$1",
            "$ 1,234",
            "$+1,234",
            "$-1,234",
            std::string("$") + "\xE2\x88\x92" + "1,234",
            "$ 1.25",
            "$+1.25",
            "$-1.25",
            std::string("$") + "\xE2\x88\x92" + "1.25"};
        const std::vector<std::string> connector_tails = {"#2", " #2", "/2", " /2", "%2", " %2"};
        auto currency_segmented = auto_options;
        currency_segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto check_atomic_currency = [&](const std::string& input,
                                               const TextFrontendOptions& options) {
            const auto result = frontend.process(input, options);
            const auto unresolved = std::find_if(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            return result.normalized_text == input && unresolved != result.warnings.end() &&
                   std::count_if(result.warnings.begin(),
                                 result.warnings.end(),
                                 [](const TextWarning& warning) {
                                     return warning.code == WarningCode::UnresolvedNumber;
                                 }) == 1 &&
                   unresolved->offset == 0 && unresolved->length == input.size();
        };
        for (const auto& prefix : currency_prefixes) {
            for (const auto& options : {en, ru, auto_options, currency_segmented})
                CHECK(check_atomic_currency(prefix, options));
            for (const auto& tail : connector_tails) {
                const auto input = prefix + tail;
                for (const auto& options : {en, ru, auto_options, currency_segmented})
                    CHECK(check_atomic_currency(input, options));
            }
        }
        const std::string adjacent = "$-1 /2 $-1 #2";
        const auto adjacent_result = frontend.process(adjacent, en);
        CHECK(adjacent_result.normalized_text == adjacent);
        CHECK(adjacent_result.warnings.size() == 2);
        CHECK(adjacent_result.warnings[0].code == WarningCode::UnresolvedNumber);
        CHECK(adjacent_result.warnings[0].offset == 0);
        CHECK(adjacent_result.warnings[0].length == std::string("$-1 /2").size());
        CHECK(adjacent_result.warnings[1].code == WarningCode::UnresolvedNumber);
        CHECK(adjacent_result.warnings[1].offset == std::string("$-1 /2 ").size());
        CHECK(adjacent_result.warnings[1].length == std::string("$-1 #2").size());
    }
    {
        const auto genitive_21 = frontend.process("после 21 попыток.", ru);
        CHECK(genitive_21.normalized_text == "после 21 попыток.");
        CHECK(genitive_21.warnings.size() == 1);
        CHECK(genitive_21.warnings.front().offset == genitive_21.original_text.find("21"));
        CHECK(genitive_21.warnings.front().length == 2);
        const auto genitive_21_nodes = frontend.process("из 21 узлов.", ru);
        CHECK(genitive_21_nodes.normalized_text == "из 21 узлов.");
        CHECK(genitive_21_nodes.warnings.size() == 1);
        const auto malformed_genitive = frontend.process("после 21.5 попыток.", ru);
        CHECK(malformed_genitive.normalized_text == "после 21.5 попыток.");
        CHECK(malformed_genitive.warnings.size() == 1);
        CHECK(malformed_genitive.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(malformed_genitive.warnings.front().offset ==
              malformed_genitive.original_text.find("21.5"));
        CHECK(malformed_genitive.warnings.front().length == 4);
    }
    {
        TextFrontendOptions segmented = auto_options;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const std::vector<std::string> unsupported_compounds = {"после 2 %",
                                                                "из 2 %",
                                                                "после 2 % 3",
                                                                "после 2 %foo",
                                                                "после 2 %foo 3",
                                                                "после 2 %RTX-4090",
                                                                "после 2кг",
                                                                "после 2 кг",
                                                                "после 2 kgfoo",
                                                                "после 2GBfoo",
                                                                "после 2 руб.foo",
                                                                "после 2GB",
                                                                "после 2 руб."};
        for (const auto& input : unsupported_compounds) {
            const std::array<std::pair<const char*, TextFrontendOptions>, 3> modes = {
                std::pair{"ru", ru},
                std::pair{"auto", auto_options},
                std::pair{"segment", segmented}};
            for (const auto& [mode, options] : modes) {
                const auto result = frontend.process(input, options);
                CHECK(result.original_text == input);
                if (input == "после 2GB" && mode == std::string("segment")) {
                    CHECK(result.normalized_text == "после two gigabytes");
                    CHECK(result.pronunciation_text == "после two gigabytes");
                    CHECK(std::count_if(result.warnings.begin(),
                                        result.warnings.end(),
                                        [](const auto& warning) {
                                            return warning.code ==
                                                   WarningCode::AmbiguousNormalization;
                                        }) == 1);
                    CHECK(std::none_of(
                        result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                            return warning.code == WarningCode::UnresolvedNumber;
                        }));
                    continue;
                }
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                const auto unresolved = std::count_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved == 1);
                const auto& warning = *std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(warning.offset == input.find('2'));
                CHECK(warning.length == input.size() - input.find('2'));
            }
        }
        CHECK(frontend.process("после 2, 3", ru).normalized_text == "после двух, три");
        CHECK(frontend.process("после 2 %.", ru).normalized_text == "после 2 %.");
        CHECK(frontend.process("after 2 %", en).normalized_text == "after two percent");
        CHECK(frontend.process("after 2 kg", en).normalized_text == "after two kilograms");
        CHECK(frontend.process("after 2 GB", en).normalized_text == "after two gigabytes");
        const std::vector<std::string> decimal_genitive_compounds = {
            "после 2,5 %", "после 2,5 % 3", "после 2,5 кг", "после 2,5 руб.", "после -2,5 %"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : decimal_genitive_compounds) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                const auto& warning = result.warnings.front();
                CHECK(warning.code == WarningCode::UnresolvedNumber);
                const auto begin = input.find(input.find('-') == std::string::npos ? '2' : '-');
                CHECK(warning.offset == begin);
                CHECK(warning.length == input.size() - begin);
            }
        }
        const std::vector<std::string> completed_unit_tails = {
            "после 2 кг 3", "после 2GB 3", "после 2,5 кг 3"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : completed_unit_tails) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                const auto expected = input.find("GB") != std::string::npos && options == &segmented
                                          ? "после two gigabytes три"
                                          : input.substr(0, input.rfind('3')) + "три";
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == result.normalized_text);
                const auto unresolved = std::count_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                if (input.find("GB") != std::string::npos && options == &segmented) {
                    CHECK(unresolved == 0);
                    CHECK(result.warnings.size() == 1);
                    CHECK(result.warnings.front().code == WarningCode::AmbiguousNormalization);
                    continue;
                }
                CHECK(unresolved == 1);
                const auto& warning = *std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& item) {
                        return item.code == WarningCode::UnresolvedNumber;
                    });
                const auto begin = input.find(input.find('-') == std::string::npos ? '2' : '-');
                CHECK(warning.offset == begin);
                CHECK(warning.length == input.rfind(" 3") - begin);
            }
        }
        const std::vector<std::string> foreign_decimal_compounds = {"после 2.5 % 3",
                                                                    "после 2.5 кг"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : foreign_decimal_compounds) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                const auto begin = input.find('2');
                CHECK(result.warnings.front().offset == begin);
                CHECK(result.warnings.front().length == input.size() - begin);
            }
        }
        const std::vector<std::string> attached_decimal_tails = {"после 2.5 kgfoo",
                                                                 "после 2.5 GBfoo",
                                                                 "после 2.5 руб.foo",
                                                                 "после 2,5 kgfoo",
                                                                 "после -2.5 kgfoo",
                                                                 "после -2,5 kgfoo"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : attached_decimal_tails) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                const auto unresolved = std::count_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved == 1);
                const auto& warning = *std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& item) {
                        return item.code == WarningCode::UnresolvedNumber;
                    });
                const auto begin =
                    input.find('-') == std::string::npos ? input.find('2') : input.find('-');
                CHECK(warning.offset == begin);
                CHECK(warning.length == input.size() - begin);
            }
        }
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            const std::string input = "после 2 руб.../2";
            const auto result = frontend.process(input, *options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            const auto unresolved = std::count_if(
                result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved == 1);
            const auto& warning =
                *std::find_if(result.warnings.begin(), result.warnings.end(), [](const auto& item) {
                    return item.code == WarningCode::UnresolvedNumber;
                });
            const auto begin = input.find('2');
            CHECK(warning.offset == begin);
            CHECK(warning.length == input.size() - begin);
        }
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            const std::string input = "после 2 руб... / 2";
            const auto result = frontend.process(input, *options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            const auto unresolved = std::count_if(
                result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                    return warning.code == WarningCode::UnresolvedNumber;
                });
            CHECK(unresolved == 1);
            const auto& warning =
                *std::find_if(result.warnings.begin(), result.warnings.end(), [](const auto& item) {
                    return item.code == WarningCode::UnresolvedNumber;
                });
            const auto begin = input.find('2');
            CHECK(warning.offset == begin);
            CHECK(warning.length == input.size() - begin);
        }
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input :
                 {std::string("после 2\xCC\x81 кгfoo"), std::string("после 2\xCC\x81 %foo")}) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                const auto unresolved = std::count_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved == 1);
                const auto& warning = *std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& item) {
                        return item.code == WarningCode::UnresolvedNumber;
                    });
                const auto begin = input.find('2');
                CHECK(warning.offset == begin);
                CHECK(warning.length == input.size() - begin);
            }
        }
        const std::vector<std::string> combining_connector_matrix = {
            "после 2\xCC\x81 / 2",
            "после 2\xCC\x81 % 2",
            "после 2\xCC\x81 \xE2\x80\x94 2",
            "после 2\xCC\x81 кг / 2"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : combining_connector_matrix) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                const auto begin = input.find('2');
                CHECK(result.warnings.front().offset == begin);
                CHECK(result.warnings.front().length == input.size() - begin);
            }
        }
        const std::vector<std::string> combining_suffix_matrix = {"после 2\xCC\x81 кг",
                                                                  "после 2\xCC\x81 попыток",
                                                                  "после 2\xCC\x81 ошибок",
                                                                  "после 2\xCC\x81 руб.",
                                                                  "после 2\xCC\x81 руб.../2",
                                                                  "после 2\xCC\x81 %",
                                                                  "после 2\xCC\x81 кг!"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : combining_suffix_matrix) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                const auto begin = input.find('2');
                CHECK(result.warnings.front().offset == begin);
                const auto expected_end = input.back() == '!' ? input.size() - 1 : input.size();
                CHECK(result.warnings.front().length == expected_end - begin);
            }
        }
        struct CombiningAdmissionCase {
            const char* input;
            const char* normalized;
            std::size_t warning_length;
        };
        const std::vector<CombiningAdmissionCase> combining_admission_matrix = {
            {"после 2 м\xCC\x81", "после 2 м\xCC\x81", 6},
            {"после 2 м\xCC\x81 3", "после 2 м\xCC\x81 три", 6},
            {"после 2 руб\xCC\x81.", "после 2 руб\xCC\x81.", 11},
            {"после 2,5\xCC\x81 кг", "после 2,5\xCC\x81 кг", 10},
            {"после 2.5\xCC\x81 кг", "после 2.5\xCC\x81 кг", 10}};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& test : combining_admission_matrix) {
                const auto result = frontend.process(test.input, *options);
                CHECK(result.original_text == test.input);
                CHECK(result.normalized_text == test.normalized);
                CHECK(result.pronunciation_text == test.normalized);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.front().offset == result.original_text.find('2'));
                CHECK(result.warnings.front().length == test.warning_length);
            }
        }
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            const auto result = frontend.process("после 2 рубежей", *options);
            CHECK(result.normalized_text == "после двух рубежей");
            CHECK(result.pronunciation_text == result.normalized_text);
            CHECK(result.warnings.empty());
        }
        const std::vector<std::pair<std::string, std::string>> decomposed_lexical_controls = {
            {"после 2 руб\xCC\x81ежей", "после двух руб\xCC\x81ежей"},
            {"после 2 руб\xCC\x81инов", "после двух руб\xCC\x81инов"},
            {"после 2 метр\xCC\x81ов", "после двух метр\xCC\x81ов"},
            {"после 2 метр\xCC\x81ополитенов", "после двух метр\xCC\x81ополитенов"}};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& [input, expected] : decomposed_lexical_controls) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == expected);
                CHECK(result.warnings.empty());
            }
        }
        const std::vector<std::pair<std::string, std::size_t>> short_unit_combining = {
            {"2 м\xCC\x81", std::string("2 м\xCC\x81").size()},
            {"2 м\xCC\x81"
             "foo",
             (std::string("2 м\xCC\x81") + "foo").size()},
            {"2 м\xCC\x81.", std::string("2 м\xCC\x81").size()}};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& [input, warning_length] : short_unit_combining) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                if (input.find("foo") == std::string::npos)
                    CHECK(result.warnings.size() == 1);
                CHECK(std::count_if(
                          result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                              return warning.code == WarningCode::UnresolvedNumber;
                          }) == 1);
                const auto unresolved = std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved != result.warnings.end());
                CHECK(unresolved->offset == 0);
                CHECK(unresolved->length == warning_length);
            }
        }
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& [input, expected] : std::vector<std::pair<std::string, std::string>>{
                     {"2 месяцев", "два месяцев"}, {"2 м\xCC\x81ежей", "два м\xCC\x81ежей"}}) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == expected);
                CHECK(result.warnings.empty());
            }
        }
        const std::vector<std::string> decimal_separators = {".", ","};
        const std::vector<std::string> numeric_signs = {"", "+", "-", "\xE2\x88\x92"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& sign : numeric_signs) {
                for (const auto& separator : decimal_separators) {
                    const auto input = sign + "2" + separator + "5 м\xCC\x81";
                    const auto result = frontend.process(input, *options);
                    CHECK(result.original_text == input);
                    CHECK(result.normalized_text == input);
                    CHECK(result.pronunciation_text == input);
                    CHECK(result.warnings.size() == 1);
                    CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                    CHECK(result.warnings.front().offset == 0);
                    CHECK(result.warnings.front().length == input.size());
                }
            }
        }
        std::vector<std::string> combining_short_unit_tails = {"2 м\xCC\x81%1",
                                                               "2 м\xCC\x81"
                                                               "1",
                                                               "2 м\xCC\x81/1",
                                                               "+2.5 м\xCC\x81%1"};
        const std::vector<std::string> combining_short_unit_continuations = {
            "%1", "/1", "#1", "$1", "foo"};
        for (const auto& sign : numeric_signs) {
            for (const auto& separator : decimal_separators) {
                for (const auto& continuation : combining_short_unit_continuations) {
                    combining_short_unit_tails.push_back(sign + "2" + separator + "5 м\xCC\x81" +
                                                         continuation);
                }
            }
        }
        const std::vector<std::string> combining_connector_tails = {"+2 %\xCC\x81",
                                                                    "2 /\xCC\x81"
                                                                    "3",
                                                                    "2 #\xCC\x81"
                                                                    "3",
                                                                    "2 $\xCC\x81"
                                                                    "3"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : combining_short_unit_tails) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(std::count_if(
                          result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                              return warning.code == WarningCode::UnresolvedNumber;
                          }) == 1);
                const auto unresolved = std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved != result.warnings.end());
                CHECK(unresolved->offset == 0);
                CHECK(unresolved->length == input.size());
            }
            for (const auto& input : combining_connector_tails) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.front().offset == 0);
                CHECK(result.warnings.front().length == input.size());
            }
        }
        const std::vector<std::string> combining_chain_cases = {"2 м\xCC\x81.%1",
                                                                "2 кг\xCC\x81.%1",
                                                                "2 руб\xCC\x81.%1",
                                                                "2,5\xCC\x81 кг%1",
                                                                "2,5\xCC\x81 #1",
                                                                "2 м\xCC\x81.+1",
                                                                "после 2,5 руб\xCC\x81.%1"};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& input : combining_chain_cases) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.pronunciation_text == input);
                CHECK(std::count_if(
                          result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                              return warning.code == WarningCode::UnresolvedNumber;
                          }) == 1);
                const auto unresolved = std::find_if(
                    result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                        return warning.code == WarningCode::UnresolvedNumber;
                    });
                CHECK(unresolved != result.warnings.end());
                const auto begin = input.find("2");
                CHECK(unresolved->offset == begin);
                CHECK(unresolved->length == input.size() - begin);
            }
        }
        const std::vector<std::pair<std::string, std::string>> english_combining_controls = {
            {"2 с\xCC\x81", "two с\xCC\x81"}, {"2 а\xCC\x81.", "two а\xCC\x81."}};
        for (const auto& [input, expected] : english_combining_controls) {
            const auto result = frontend.process(input, en);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == expected);
            CHECK(result.pronunciation_text == expected);
            CHECK(result.warnings.empty());
        }
        const auto english_combining_chain = frontend.process("2,5\xCC\x81 #1", en);
        CHECK(english_combining_chain.normalized_text == english_combining_chain.original_text);
        CHECK(english_combining_chain.pronunciation_text == english_combining_chain.original_text);
        CHECK(std::count_if(english_combining_chain.warnings.begin(),
                            english_combining_chain.warnings.end(),
                            [](const auto& warning) {
                                return warning.code == WarningCode::UnresolvedNumber;
                            }) == 1);
        const auto english_unresolved = std::find_if(
            english_combining_chain.warnings.begin(),
            english_combining_chain.warnings.end(),
            [](const auto& warning) { return warning.code == WarningCode::UnresolvedNumber; });
        CHECK(english_unresolved != english_combining_chain.warnings.end());
        CHECK(english_unresolved->offset == 0);
        CHECK(english_unresolved->length == english_combining_chain.original_text.size());
        const std::vector<std::pair<std::string, std::string>> unit_punctuation = {
            {"после 2 кг!", "после 2 кг!"},
            {"после 2,5 кг, 3", "после 2,5 кг, три"},
            {"после 2.5 кг.", "после 2.5 кг."}};
        for (const auto* options : {&ru, &auto_options, &segmented}) {
            for (const auto& [input, expected] : unit_punctuation) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == expected);
                CHECK(result.pronunciation_text == expected);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                const auto begin = input.find('2');
                CHECK(result.warnings.front().offset == begin);
                const auto expected_length =
                    input.find("2,5") != std::string::npos   ? std::string("2,5 кг").size()
                    : input.find("2.5") != std::string::npos ? std::string("2.5 кг").size()
                                                             : std::string("2 кг").size();
                CHECK(result.warnings.front().length == expected_length);
            }
        }
        // A number after a completed currency surface is a separate token;
        // only the unsupported compound itself is preserved atomically.
        {
            const std::string input = "после 2 руб. 3";
            const auto result = frontend.process(input, ru);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == "после 2 руб. три");
            CHECK(result.pronunciation_text == result.normalized_text);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == input.find('2'));
            CHECK(result.warnings.front().length == std::string("2 руб.").size());
        }
        for (const auto& input : {std::string("https://example.com/2GB"),
                                  std::string("user2%@example.com"),
                                  std::string("RTX-2GB")}) {
            const auto result = frontend.process(input, ru);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.empty());
        }
    }
    for (const auto& punctuation : {std::string("!"), std::string("?"), std::string(")")}) {
        const auto input = "1 руб." + punctuation;
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == "один рубль" + punctuation);
    }
    CHECK(frontend.process("1 руб...,", ru).normalized_text == "один рубль...,");
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto mixed_unit = frontend.process("Use 1,5 кг", segmented);
        CHECK(mixed_unit.normalized_text == "Use одна целая пять десятых килограмма");
        CHECK(!mixed_unit.warnings.empty());
        const auto mixed_gb = frontend.process("Используем 1,5 GB данных.", segmented);
        CHECK(mixed_gb.normalized_text == "Используем одна целая пять десятых гигабайта данных.");
        const auto foreign_percent = frontend.process("Порог 0.5% задан для RTX-4090.", segmented);
        CHECK(foreign_percent.normalized_text == "Порог 0.5% задан для RTX-4090.");
        CHECK(foreign_percent.warnings.size() == 1);
        CHECK(foreign_percent.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(foreign_percent.warnings.front().offset == 11);
        CHECK(foreign_percent.warnings.front().length == 4);
    }
    for (const auto& input : {std::string("1.5%/4"),
                              std::string("1.5%#4"),
                              std::string("1.5%$4"),
                              std::string("1.5%word")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    {
        const auto result = frontend.process("Порог 1.5% 123 задан", ru);
        CHECK(result.normalized_text == "Порог 1.5% 123 задан");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().offset == std::string("Порог ").size());
        CHECK(result.warnings.front().length == std::string("1.5% 123").size());
    }
    for (const auto& input : {std::string("https://example.com/1.5%"),
                              std::string("user1.5%@example.com"),
                              std::string("RTX-1.5%")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.empty());
    }
    for (const auto& input : {std::string("February 29, 2020"), std::string("February 29, 2024")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text != input);
        CHECK(result.warnings.empty());
    }
    {
        const auto result = frontend.process("February 29, 2025", en);
        CHECK(result.normalized_text == "February 29, 2025");
        CHECK(result.warnings.size() == 1);
    }
    for (const auto& input : {std::string("February 29, 2025foo"),
                              std::string("February 29, 2024#4"),
                              std::string("February 29, 2024/4")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    // A connector without a lexical continuation is a hard boundary.  These
    // cases must return promptly rather than retrying the same connector
    // indefinitely.  The date and following number remain independent tokens.
    for (const auto& input : {std::string("February 29, 2024# 4"),
                              std::string("February 29, 2024/ 4"),
                              std::string("February 29, 2024+ 4"),
                              std::string("February 29, 2024- 4"),
                              std::string("February 29, 2024. 4")}) {
        const auto result = frontend.process(input, en);
        CHECK(result.normalized_text.find("February twenty ninth") == 0);
        CHECK(result.normalized_text.find("four") != std::string::npos);
    }
    {
        for (const auto* options : {&en, &ru, &auto_options}) {
            for (const auto& input : {std::string("1.5%RTX- 4090"),
                                      std::string("1.5%RTX+ 4090"),
                                      std::string("1.5%RTX-"),
                                      std::string("1.5%RTX+"),
                                      std::string("1.5%RTX."),
                                      std::string("1.5%RTX/"),
                                      std::string("1.5%RTX#"),
                                      std::string("1.5%RTX$")}) {
                const auto result = frontend.process(input, *options);
                CHECK(result.normalized_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.front().offset == 0);
                CHECK(result.warnings.front().length == input.size());
            }
            const std::string lexical_suffix = "1.5%RTX- foo";
            const auto result = frontend.process(lexical_suffix, *options);
            CHECK(result.normalized_text == lexical_suffix);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == lexical_suffix.find(' '));
        }
    }
    {
        const std::vector<std::string> unicode_connector_cases = {
            std::string("1.5%RTX") + "\xE2\x88\x92" + " 4090",
            std::string("1.5%RTX") + "\xE2\x80\x93" + " 4090",
            std::string("1.5%RTX") + "\xE2\x80\x94" + " 4090",
            "1.5%RTX= 4090",
            "1.5%RTX* 4090"};
        for (const auto* options : {&en, &ru, &auto_options}) {
            for (const auto& input : unicode_connector_cases) {
                const auto result = frontend.process(input, *options);
                CHECK(result.normalized_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                CHECK(result.warnings.front().offset == 0);
                CHECK(result.warnings.front().length == input.size());
            }
        }
    }
    for (const auto& input : {std::string("1 руб...foo"), std::string("1,2 руб.../2")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == input.size());
    }
    struct RussianAgreementCase {
        const char* input;
        const char* expected;
        bool preserve;
    };
    const std::vector<RussianAgreementCase> genitive_cases = {
        {"после 1 попыток.", "после 1 попыток.", true},
        {"после 2 попыток.", "после двух попыток.", false},
        {"после 5 попыток.", "после пяти попыток.", false},
        {"после 21 попыток.", "после 21 попыток.", true},
        {"после 22 попыток.", "после двадцати двух попыток.", false},
        {"после 42 ошибок.", "после сорока двух ошибок.", false},
        {"после 100 попыток.", "после ста попыток.", false},
        {"после 2 месяцев.", "после двух месяцев.", false},
        {"после 2 минут.", "после двух минут.", false},
        {"после 2 машин.", "после двух машин.", false},
        {"после 2 метров.", "после двух метров.", false},
        {"После 2 попыток.", "После двух попыток.", false},
        {"Из 5 узлов.", "Из пяти узлов.", false},
        {"из 42 файлов.", "из сорока двух файлов.", false},
        {"из 100 узлов.", "из ста узлов.", false},
        {"из 21 узлов.", "из 21 узлов.", true},
    };
    for (const auto& test : genitive_cases) {
        const auto result = frontend.process(test.input, ru);
        CHECK(result.original_text == test.input);
        CHECK(result.normalized_text == test.expected);
        CHECK(result.pronunciation_text == test.expected);
        if (test.preserve) {
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        } else {
            CHECK(result.warnings.empty());
        }
    }
    {
        const std::vector<std::pair<std::string, std::string>> embedded_contexts = {
            {"вниз 2,5", "вниз две целых пять десятых"},
            {"эскиз 2", "эскиз два"},
            {"непосле 2", "непосле два"}};
        for (const auto& test : embedded_contexts) {
            const auto result = frontend.process(test.first, ru);
            CHECK(result.normalized_text == test.second);
            CHECK(result.warnings.empty());
        }
    }
    {
        const std::vector<std::string> unsupported_surfaces = {"после 2%",
                                                               "после 2 кг",
                                                               "после 2 руб.",
                                                               "после 2 GB",
                                                               "после 2:30",
                                                               "после 2,5",
                                                               "после 2,foo",
                                                               "после 2:foo",
                                                               "после 2-3",
                                                               "после 2 reports"};
        for (const auto& input : unsupported_surfaces) {
            const auto result = frontend.process(input, ru);
            const auto number_begin = input.find('2');
            CHECK(result.normalized_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == number_begin);
            CHECK(result.warnings.front().length == input.size() - number_begin);
        }
    }
    {
        const std::vector<std::pair<std::string, std::string>> combining_contexts = {
            {"x\xCC\x81из 2", "x\xCC\x81из два"}, {"x1из 2", "x1из два"}};
        for (const auto& test : combining_contexts) {
            const auto result = frontend.process(test.first, ru);
            CHECK(result.normalized_text == test.second);
            CHECK(std::none_of(
                result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                    return warning.code == WarningCode::AmbiguousNormalization;
                }));
        }
    }
    {
        const auto result = frontend.process("после 11 попыток", ru);
        CHECK(result.normalized_text == "после одиннадцати попыток");
        CHECK(result.warnings.empty());
    }
    {
        const std::vector<std::pair<std::string, std::string>> punctuation_cases = {
            {"после 2.", "после двух."},
            {"из 2,", "из двух,"},
            {"после 2:", "после двух:"},
            {"после 2, 3", "после двух, три"}};
        for (const auto& test : punctuation_cases) {
            const auto result = frontend.process(test.first, ru);
            CHECK(result.normalized_text == test.second);
            CHECK(result.warnings.empty());
        }
    }
    {
        const std::vector<std::string> spaced_connector_cases = {"после 2/ 3",
                                                                 "после 2 + 3",
                                                                 "после 2 / 3",
                                                                 "после 2# 3",
                                                                 "после 2- 3",
                                                                 "после 2= 3",
                                                                 "после 2* 3",
                                                                 "после 2$ 3"};
        for (const auto* options : {&ru, &auto_options}) {
            for (const auto& input : spaced_connector_cases) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text == input);
                CHECK(result.warnings.size() == 1);
                CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
                const auto begin = input.find('2');
                CHECK(result.warnings.front().offset == begin);
                CHECK(result.warnings.front().length == input.size() - begin);
            }
        }
    }
    const std::vector<RussianAgreementCase> temporal_cases = {
        {"за 1 год.", "за один год.", false},
        {"за 2 года.", "за два года.", false},
        {"за 5 лет.", "за пять лет.", false},
        {"за 21 день.", "за двадцать один день.", false},
        {"за 22 дня.", "за двадцать два дня.", false},
        {"за 42 дня.", "за сорок два дня.", false},
        {"За 100 лет.", "За сто лет.", false},
    };
    for (const auto& test : temporal_cases) {
        const auto result = frontend.process(test.input, ru);
        CHECK(result.original_text == test.input);
        CHECK(result.normalized_text == test.expected);
        CHECK(result.pronunciation_text == test.expected);
        CHECK(result.warnings.empty());
    }
    for (const auto& input : {std::string("после 2,5 попыток."),
                              std::string("после 2.5 попыток."),
                              std::string("из 21.5 узлов.")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        const auto token = input.find("21.5") != std::string::npos  ? std::string("21.5")
                           : input.find("2.5") != std::string::npos ? std::string("2.5")
                                                                    : std::string("2,5");
        CHECK(result.warnings.front().offset == input.find(token));
        CHECK(result.warnings.front().length >= token.size());
    }
    {
        const std::string input = "после 2:30 попыток.";
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == input.find("2:30"));
        CHECK(result.warnings.front().length == std::string("2:30").size());
    }
    for (const auto& input : {std::string("https://example.com/2"),
                              std::string("user2@example.com"),
                              std::string("RTX-2")}) {
        const auto result = frontend.process(input, ru);
        CHECK(result.normalized_text == input);
        CHECK(result.warnings.empty());
    }
    {
        TextFrontendOptions segmented;
        segmented.language = Language::Auto;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result = frontend.process("после 2 reports.", segmented);
        CHECK(result.original_text == "после 2 reports.");
        CHECK(result.normalized_text == "после two reports.");
        CHECK(
            std::count_if(result.warnings.begin(), result.warnings.end(), [](const auto& warning) {
                return warning.code == WarningCode::AmbiguousNormalization;
            }) == 1);
    }
    {
        const auto result = frontend.process("из-за 2 узлов.", ru);
        CHECK(result.normalized_text == "из-за два узлов.");
        CHECK(result.warnings.empty());
    }
    {
        const auto result = frontend.process("до 1,2 градусов.", ru);
        CHECK(result.normalized_text == "до 1,2 градусов.");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == std::string("до ").size());
        CHECK(result.warnings.front().length == std::string("1,2").size());
    }
    {
        const auto result = frontend.process("1.5%RTX-4090", ru);
        CHECK(result.normalized_text == "1.5%RTX-4090");
        CHECK(result.warnings.size() == 1);
        CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
        CHECK(result.warnings.front().offset == 0);
        CHECK(result.warnings.front().length == result.original_text.size());
    }
    {
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result = frontend.process("Use 1,5 кгfoo", segmented);
        CHECK(result.normalized_text == "Use 1,5 кгfoo");
        CHECK(std::count_if(
                  result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                      return warning.code == WarningCode::UnresolvedNumber;
                  }) == 1);
        CHECK(std::any_of(
            result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                return warning.code == WarningCode::AmbiguousNormalization;
            }));
    }
    {
        // The document-level mixed-language diagnostic is independent from
        // a candidate-specific unresolved warning in another span.
        TextFrontendOptions segmented;
        segmented.mixed_language_policy = MixedLanguagePolicy::SegmentCandidates;
        const auto result = frontend.process("Используем 2 GB и 3 kgfoo", segmented);
        CHECK(result.normalized_text == "Используем two gigabytes и 3 kgfoo");
        CHECK(std::count_if(
                  result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                      return warning.code == WarningCode::AmbiguousNormalization;
                  }) == 1);
        CHECK(std::count_if(
                  result.warnings.begin(), result.warnings.end(), [](const TextWarning& warning) {
                      return warning.code == WarningCode::UnresolvedNumber;
                  }) == 1);
    }
    std::string many_protected;
    for (int i = 0; i < 35; ++i) {
        if (!many_protected.empty())
            many_protected += ' ';
        many_protected += "https://example.com/item" + std::to_string(i);
    }
    CHECK(frontend.process(many_protected, en).normalized_text == many_protected);
    std::string invalid("\xc0\xaf", 2);
    const auto invalid_result = frontend.process(invalid, en);
    CHECK(invalid_result.warnings.front().code == WarningCode::InvalidUtf8);
    CHECK(invalid_result.warnings.front().offset == 0);
    CHECK(invalid_result.warnings.front().length == invalid.size());
    std::string surrogate("\xed\xa0\x80", 3);
    CHECK(frontend.process(surrogate, en).warnings.front().code == WarningCode::InvalidUtf8);
    std::string too_large("\xf4\x90\x80\x80", 4);
    CHECK(frontend.process(too_large, en).warnings.front().code == WarningCode::InvalidUtf8);
    const std::string emoji("\xf0\x9f\x98\x80", 4);
    CHECK(frontend.process(emoji, en).warnings.empty());
    std::string bad_continuation("\xe2\x28\xa1", 3);
    CHECK(frontend.process(bad_continuation, en).warnings.front().code == WarningCode::InvalidUtf8);
    std::string truncated("\xf0\x9f\x98", 3);
    CHECK(frontend.process(truncated, en).warnings.front().code == WarningCode::InvalidUtf8);
    {
        // Adversarial scanner matrix: every connector shape is exercised with
        // whitespace, a terminal boundary, technical identifiers, URLs and
        // email-like text.  The public API must return promptly, preserve the
        // source bytes and never emit overlapping unresolved spans.
        const std::vector<std::string> connectors = {
            "-", "+", ".", "/", "#", "$", "=", "*", "\xE2\x88\x92", "\xE2\x80\x93", "\xE2\x80\x94"};
        const std::vector<std::string> suffixes = {" 4090", "", " foo"};
        const std::vector<std::string> prefixes = {"1.5%RTX", "abc1.5%RTX"};
        const auto utf8_boundary = [](const std::string& value, std::size_t offset) {
            return offset <= value.size() &&
                   (offset == value.size() ||
                    (static_cast<unsigned char>(value[offset]) & 0xC0U) != 0x80U);
        };
        for (const auto* options : {&en, &ru, &auto_options}) {
            for (const auto& prefix : prefixes) {
                for (const auto& connector : connectors) {
                    for (const auto& suffix : suffixes) {
                        const auto input = prefix + connector + suffix;
                        const auto result = frontend.process(input, *options);
                        CHECK(result.original_text == input);
                        CHECK(result.normalized_text.find('\x01') == std::string::npos);
                        CHECK(result.normalized_text.find('\x02') == std::string::npos);
                        CHECK(result.pronunciation_text.find('\x01') == std::string::npos);
                        CHECK(result.pronunciation_text.find('\x02') == std::string::npos);
                        std::vector<std::pair<std::size_t, std::size_t>> spans;
                        for (const auto& warning : result.warnings) {
                            CHECK(warning.offset <= input.size());
                            CHECK(warning.length <= input.size() - warning.offset);
                            CHECK(utf8_boundary(input, warning.offset));
                            CHECK(utf8_boundary(input, warning.offset + warning.length));
                            if (warning.code == WarningCode::UnresolvedNumber && warning.length)
                                spans.emplace_back(warning.offset, warning.offset + warning.length);
                        }
                        std::sort(spans.begin(), spans.end());
                        for (std::size_t index = 1; index < spans.size(); ++index)
                            CHECK(spans[index - 1].second <= spans[index].first);
                    }
                }
            }
            for (const auto& input : {std::string("https://example.com/1.5%RTX- 4090"),
                                      std::string("user1.5%@example.com")}) {
                const auto result = frontend.process(input, *options);
                CHECK(result.original_text == input);
                CHECK(result.normalized_text.find('\x01') == std::string::npos);
                CHECK(result.normalized_text.find('\x02') == std::string::npos);
            }
        }
    }
    {
        const auto arithmetic_matches =
            [&](const TextFrontendOptions& options,
                const std::vector<std::pair<std::string, std::string>>& cases) {
                for (const auto& [input, expected] : cases) {
                    const auto result = frontend.process(input, options);
                    if (result.original_text != input || !result.warnings.empty() ||
                        result.normalized_text != expected || result.pronunciation_text != expected)
                        return false;
                }
                return true;
            };
        const std::vector<std::pair<std::string, std::string>> english_cases = {
            {"1 / 2", "one / two"},
            {"1 = 2", "one = two"},
            {"1 * 2", "one * two"},
            {"1 + 2 = 3", "one + two = three"},
            {"1 / 2 = 0.5", "one / two = zero point five"}};
        const std::vector<std::pair<std::string, std::string>> russian_expected = {
            {"1 / 2", "\xD0\xBE\xD0\xB4\xD0\xB8\xD0\xBD / \xD0\xB4\xD0\xB2\xD0\xB0"},
            {"1 = 2", "\xD0\xBE\xD0\xB4\xD0\xB8\xD0\xBD = \xD0\xB4\xD0\xB2\xD0\xB0"},
            {"1 * 2", "\xD0\xBE\xD0\xB4\xD0\xB8\xD0\xBD * \xD0\xB4\xD0\xB2\xD0\xB0"},
            {"1 + 2 = 3",
             "\xD0\xBE\xD0\xB4\xD0\xB8\xD0\xBD + \xD0\xB4\xD0\xB2\xD0\xB0 = "
             "\xD1\x82\xD1\x80\xD0\xB8"},
            {"1 / 2 = 0.5", "\xD0\xBE\xD0\xB4\xD0\xB8\xD0\xBD / \xD0\xB4\xD0\xB2\xD0\xB0 = 0.5"}};
        CHECK(arithmetic_matches(en, english_cases));
        CHECK(arithmetic_matches(ru, russian_expected));
        CHECK(arithmetic_matches(auto_options, english_cases));
    }
    for (const auto* options : {&ru, &auto_options, &segmented}) {
        for (const auto& input : {std::string("2,5\xCC\x81 кг"), std::string("2 kg\xCC\x81")}) {
            const auto result = frontend.process(input, *options);
            CHECK(result.original_text == input);
            CHECK(result.normalized_text == input);
            CHECK(result.pronunciation_text == input);
            CHECK(result.warnings.size() == 1);
            CHECK(result.warnings.front().code == WarningCode::UnresolvedNumber);
            CHECK(result.warnings.front().offset == 0);
            CHECK(result.warnings.front().length == input.size());
        }
    }
    const std::vector<std::pair<const TextFrontendOptions*, std::pair<std::string, std::string>>>
        decomposed_word_controls = {{&en, {"2 cafe\xCC\x81", "two cafe\xCC\x81"}},
                                    {&en, {"2 foo\xCC\x81", "two foo\xCC\x81"}},
                                    {&ru, {"2 месяца\xCC\x81", "два месяца\xCC\x81"}},
                                    {&auto_options, {"2 cafe\xCC\x81", "two cafe\xCC\x81"}},
                                    {&segmented, {"2 cafe\xCC\x81", "two cafe\xCC\x81"}}};
    for (const auto& [options, test] : decomposed_word_controls) {
        const auto result = frontend.process(test.first, *options);
        CHECK(result.original_text == test.first);
        CHECK(result.normalized_text == test.second);
        CHECK(result.pronunciation_text == test.second);
        CHECK(result.warnings.empty());
    }
    std::cout << "tts_front tests passed\n";
    return EXIT_SUCCESS;
}
