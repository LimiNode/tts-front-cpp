#include "tts_front/tts_front.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>

namespace tts_front {
namespace {

bool valid_utf8(std::string_view s) {
  for (std::size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]); std::size_t n = 0;
    if (c < 0x80) n = 1; else if ((c & 0xe0) == 0xc0) n = 2; else if ((c & 0xf0) == 0xe0) n = 3; else if ((c & 0xf8) == 0xf0) n = 4; else return false;
    if (i + n > s.size()) return false;
    for (std::size_t j = 1; j < n; ++j) if ((static_cast<unsigned char>(s[i+j]) & 0xc0) != 0x80) return false;
    i += n;
  }
  return true;
}
std::string cleanup(std::string text) {
  text = std::regex_replace(text, std::regex("[\\t\\r\\n ]+"), " ");
  text = std::regex_replace(text, std::regex(" +([,.;:!?])"), "$1");
  text = std::regex_replace(text, std::regex("([,;:!?])([^ \\n])"), "$1 $2");
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}
std::string replace_all(std::string text, const std::regex& pattern, const std::string& replacement) {
  return std::regex_replace(text, pattern, replacement);
}
template <typename Formatter>
std::string replace_decimals(std::string text, const std::regex& pattern, Formatter formatter) {
  std::string out; std::size_t pos = 0;
  for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
    const auto begin = static_cast<std::size_t>(it->position());
    const auto finish = begin + static_cast<std::size_t>(it->length());
    const bool technical = (begin > 0 && std::isalnum(static_cast<unsigned char>(text[begin - 1]))) ||
                           (finish < text.size() && text[finish] == '.');
    out.append(text, pos, begin - pos);
    out += technical ? it->str() : formatter((*it)[1].str(), (*it)[2].str());
    pos = finish;
  }
  out += text.substr(pos); return out;
}

const char* const ru_ones[] = {"ноль","один","два","три","четыре","пять","шесть","семь","восемь","девять","десять","одиннадцать","двенадцать","тринадцать","четырнадцать","пятнадцать","шестнадцать","семнадцать","восемнадцать","девятнадцать"};
const char* const ru_tens[] = {"","","двадцать","тридцать","сорок","пятьдесят","шестьдесят","семьдесят","восемьдесят","девяносто"};
const char* const ru_hundreds[] = {"","сто","двести","триста","четыреста","пятьсот","шестьсот","семьсот","восемьсот","девятьсот"};
std::string ru_under_1000(int n) {
  std::string r; if (n >= 100) { r += ru_hundreds[n/100]; n %= 100; if (n) r += " "; }
  if (n < 20) { if (n) r += ru_ones[n]; }
  else { r += ru_tens[n/10]; if (n%10) r += " " + std::string(ru_ones[n%10]); }
  return r.empty() ? "ноль" : r;
}
std::string ru_number(long long n) {
  if (n < 0) return "минус " + ru_number(-n);
  if (n < 1000) return ru_under_1000(static_cast<int>(n));
  if (n < 1000000) {
    const int th = static_cast<int>(n / 1000), rest = static_cast<int>(n % 1000);
    std::string r;
    if (th == 1) r = "одна";
    else if (th == 2) r = "две";
    else r = ru_under_1000(th);
    r += (th % 10 == 1 && th % 100 != 11) ? " тысяча" : (th % 10 >= 2 && th % 10 <= 4 && (th%100<10 || th%100>=20)) ? " тысячи" : " тысяч";
    if (rest) r += " " + ru_under_1000(rest);
    return r;
  }
  return std::to_string(n); // explicit unresolved range is handled by the caller
}
const char* const en_ones[] = {"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
const char* const en_tens[] = {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
std::string en_number(long long n) {
  if (n < 0) return "minus " + en_number(-n);
  if (n < 20) return en_ones[n];
  if (n < 100) return std::string(en_tens[n/10]) + (n%10 ? " " + std::string(en_ones[n%10]) : "");
  if (n < 1000) return std::string(en_ones[n/100]) + " hundred" + (n%100 ? " " + en_number(n%100) : "");
  if (n < 1000000) return en_number(n/1000) + " thousand" + (n%1000 ? " " + en_number(n%1000) : "");
  return std::to_string(n);
}

std::string normalize_ru(std::string text, std::vector<TextWarning>& warnings) {
  // Preserve technical tokens, while expanding unambiguous numeric forms.
  text = replace_all(text, std::regex(R"((\d)\s*:\s*(\d))"), "$1:$2");
  text = replace_all(text, std::regex(R"(\b(\d{1,3})\s+(\d{3})\b)"), "$1$2");
  text = replace_decimals(text, std::regex(R"(\b(\d+)[,](\d+)\b)"), [](const std::string& a, const std::string& b) { return a + " целых " + b; });
  text = replace_all(text, std::regex(R"((\d{1,3})\s*%)"), "$1 процентов");
  text = replace_all(text, std::regex(R"(\b(\d{1,2}):(\d{2})\b)"), "$1 часов $2 минут");
  text = replace_all(text, std::regex(R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)"), "$1.$2.$3");
  text = replace_all(text, std::regex(R"(\b(\d+)\s*(кг|км|м|см|мм|ГБ|МБ)\b)"), "$1 $2");
  const std::regex integer(R"(\b\d+\b)");
  std::string out; std::size_t pos = 0;
  for (std::sregex_iterator it(text.begin(), text.end(), integer), end; it != end; ++it) {
    out.append(text, pos, static_cast<std::size_t>(it->position()) - pos);
    const auto token = it->str();
    try { const long long n = std::stoll(token); if (n <= 999999) out += ru_number(n); else { out += token; warnings.push_back({WarningCode::UnresolvedNumber, "Russian number is outside deterministic range"}); } }
    catch (...) { out += token; warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian number"}); }
    pos = static_cast<std::size_t>(it->position() + it->length());
  }
  out += text.substr(pos);
  text = std::move(out);
  text = replace_all(text, std::regex("г\\."), "году");
  text = replace_all(text, std::regex("т\\.д\\."), "так далее");
  text = replace_all(text, std::regex("т\\.п\\."), "тому подобное");
  text = replace_all(text, std::regex("руб\\."), "рублей");
  return text;
}

std::string normalize_en(std::string text, std::vector<TextWarning>& warnings) {
  text = replace_all(text, std::regex(R"((\d)\s*:\s*(\d))"), "$1:$2");
  text = replace_all(text, std::regex(R"(\b(\d{1,3})\s+(\d{3})\b)"), "$1$2");
  text = replace_decimals(text, std::regex(R"(\b(\d+)\.(\d+)\b)"), [](const std::string& a, const std::string& b) { return a + " point " + b; });
  text = replace_all(text, std::regex(R"((\d+(?:\.\d+)?)\s*%)"), "$1 percent");
  text = replace_all(text, std::regex(R"(\$([0-9]+(?:\.[0-9]+)?))"), "$1 dollars");
  text = replace_all(text, std::regex(R"(\b(\d{1,2}):(\d{2})\b)"), "$1 hours $2 minutes");
  const std::regex integer(R"(\b\d+\b)"); std::string out; std::size_t pos = 0;
  for (std::sregex_iterator it(text.begin(), text.end(), integer), end; it != end; ++it) {
    out.append(text, pos, static_cast<std::size_t>(it->position()) - pos); const auto token = it->str();
    try { const long long n = std::stoll(token); if (n <= 999999) out += en_number(n); else { out += token; warnings.push_back({WarningCode::UnresolvedNumber, "English number is outside deterministic range"}); } }
    catch (...) { out += token; warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English number"}); }
    pos = static_cast<std::size_t>(it->position() + it->length());
  }
  out += text.substr(pos); return out;
}

bool is_word_byte(unsigned char c) { return c >= 0x80 || std::isalnum(c) || c == '+' || c == '#' || c == '_' || c == '.'; }
std::vector<std::pair<std::size_t, std::size_t>> word_spans(const std::string& text) {
  std::vector<std::pair<std::size_t, std::size_t>> spans; std::size_t i = 0;
  while (i < text.size()) { while (i < text.size() && !is_word_byte(static_cast<unsigned char>(text[i]))) ++i; const auto b=i; while (i<text.size() && is_word_byte(static_cast<unsigned char>(text[i]))) ++i; if (i>b) spans.push_back({b,i}); }
  return spans;
}

} // namespace

bool TextFrontendResult::has_uncertainty() const noexcept {
  return std::any_of(warnings.begin(), warnings.end(), [](const TextWarning& w) { return w.code == WarningCode::AmbiguousNormalization || w.code == WarningCode::UnresolvedNumber; });
}

TextFrontendResult TextFrontend::process(std::string_view input, const TextFrontendOptions& options) const {
  TextFrontendResult result; result.original_text = std::string(input);
  if (!valid_utf8(input)) { result.warnings.push_back({WarningCode::InvalidUtf8, "Input is not valid UTF-8"}); return result; }
  std::string text = options.cleanup_unicode ? cleanup(std::string(input)) : std::string(input);
  Language language = options.language;
  if (language == Language::Auto) language = std::any_of(text.begin(), text.end(), [](unsigned char c){ return c >= 0x80; }) ? Language::Russian : Language::English;
  if (options.normalize) text = language == Language::Russian ? normalize_ru(std::move(text), result.warnings) : normalize_en(std::move(text), result.warnings);
  text = cleanup(std::move(text));

  const std::string normalized = text;
  std::string rendered = text;
  if (options.apply_dictionary && options.dictionary) {
    for (const auto& entry : options.dictionary->entries()) {
      if (entry.match != PronunciationDictionary::Match::ExactPhrase) continue;
      std::size_t at = rendered.find(entry.pattern);
      while (at != std::string::npos) {
        rendered.replace(at, entry.pattern.size(), entry.pronunciation);
        result.dictionary_replacements.push_back({entry.pattern, entry.pronunciation, at});
        at = rendered.find(entry.pattern, at + entry.pronunciation.size());
      }
    }
    const auto spans = word_spans(rendered); std::string rebuilt; std::size_t cursor = 0;
    for (const auto& span : spans) {
      rebuilt.append(rendered, cursor, span.first - cursor);
      const std::string token = rendered.substr(span.first, span.second - span.first);
      const auto* entry = options.dictionary->find_token(token);
      if (entry && entry->pronunciation != token) {
        rebuilt += entry->pronunciation;
        result.dictionary_replacements.push_back({token, entry->pronunciation, span.first});
      } else rebuilt += token;
      cursor = span.second;
    }
    rebuilt += rendered.substr(cursor); rendered = std::move(rebuilt);
  }
  result.normalized_text = normalized; result.pronunciation_text = rendered;
  for (const auto& span : word_spans(normalized)) {
    const std::string token = normalized.substr(span.first, span.second-span.first); WordPronunciation word; word.surface=token;
    const auto* entry = options.dictionary && options.apply_dictionary ? options.dictionary->find_token(token) : nullptr;
    if (entry) { word.from_dictionary=true; word.stressed_vowel=entry->stressed_vowel; }
    if (options.resolve_stress && options.stress_mode == StressMode::DictionaryOnly && !word.stressed_vowel && !entry) word.stressed_vowel = std::nullopt;
    result.words.push_back(word);
    if (options.diagnostics || word.from_dictionary || word.stressed_vowel) result.stress_decisions.push_back({token, word.stressed_vowel, word.from_dictionary, word.from_dictionary ? "pronunciation dictionary" : "no deterministic stress rule"});
  }
  if (options.stress_mode == StressMode::Automatic) result.warnings.push_back({WarningCode::AutomaticStressUnavailable, "Automatic stress is not available until a parity-proven backend is added"});
  return result;
}

const char* to_string(Language language) noexcept { switch(language){case Language::Auto:return "auto";case Language::Russian:return "russian";case Language::English:return "english";} return "unknown"; }
const char* to_string(WarningCode code) noexcept { switch(code){case WarningCode::InvalidUtf8:return "invalid_utf8";case WarningCode::UnsupportedLanguage:return "unsupported_language";case WarningCode::AmbiguousNormalization:return "ambiguous_normalization";case WarningCode::UnresolvedNumber:return "unresolved_number";case WarningCode::AutomaticStressUnavailable:return "automatic_stress_unavailable";case WarningCode::DictionaryParseError:return "dictionary_parse_error";} return "unknown"; }
} // namespace tts_front
