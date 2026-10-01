#include "tts_front/tts_front.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace tts_front {
namespace {

struct CodePoint {
  std::uint32_t value = 0;
  std::size_t offset = 0;
  std::size_t length = 0;
};

bool decode_utf8(std::string_view text, std::vector<CodePoint>& output) {
  output.clear();
  for (std::size_t i = 0; i < text.size();) {
    const std::size_t start = i;
    const auto c = static_cast<unsigned char>(text[i]);
    std::uint32_t value = 0;
    std::size_t length = 0;
    if (c <= 0x7f) { value = c; length = 1; }
    else if ((c & 0xe0) == 0xc0) { value = c & 0x1f; length = 2; }
    else if ((c & 0xf0) == 0xe0) { value = c & 0x0f; length = 3; }
    else if ((c & 0xf8) == 0xf0) { value = c & 0x07; length = 4; }
    else return false;
    if (i + length > text.size()) return false;
    for (std::size_t j = 1; j < length; ++j) {
      const auto continuation = static_cast<unsigned char>(text[i + j]);
      if ((continuation & 0xc0) != 0x80) return false;
      value = (value << 6) | (continuation & 0x3f);
    }
    if ((length == 2 && value < 0x80) || (length == 3 && value < 0x800) ||
        (length == 4 && value < 0x10000) || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) return false;
    output.push_back({value, start, length});
    i += length;
  }
  return true;
}

bool is_cyrillic(std::uint32_t cp) { return cp >= 0x0400 && cp <= 0x052f; }
bool is_latin(std::uint32_t cp) { return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0x00c0 && cp <= 0x024f); }
bool is_digit(std::uint32_t cp) { return cp >= '0' && cp <= '9'; }
bool is_letter(std::uint32_t cp) { return is_cyrillic(cp) || is_latin(cp) || (cp >= 0x3400 && cp <= 0x9fff); }
bool is_word_codepoint(std::uint32_t cp) { return is_letter(cp) || is_digit(cp) || cp == '+' || cp == '#' || cp == '_' || cp == '.'; }

std::string cleanup_text(std::string text) {
  std::string result; result.reserve(text.size());
  std::vector<CodePoint> points; if (!decode_utf8(text, points)) return text;
  bool pending_space = false;
  for (const auto& point : points) {
    const bool space = point.value == ' ' || point.value == '\t' || point.value == '\n' || point.value == '\r';
    if (space) { pending_space = !result.empty(); continue; }
    const bool punctuation = point.value == ',' || point.value == '.' || point.value == ';' || point.value == ':' || point.value == '!' || point.value == '?';
    if (pending_space && !punctuation && !result.empty()) result.push_back(' ');
    pending_space = false;
    result.append(text, point.offset, point.length);
  }
  while (!result.empty() && result.back() == ' ') result.pop_back();
  return result;
}

template <typename Formatter>
std::string replace_matches(std::string text, const std::regex& pattern, Formatter formatter) {
  std::string output; std::size_t cursor = 0;
  for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
    const auto begin = static_cast<std::size_t>(it->position());
    const auto finish = begin + static_cast<std::size_t>(it->length());
    output.append(text, cursor, begin - cursor);
    output += formatter(*it);
    cursor = finish;
  }
  output += text.substr(cursor); return output;
}

const char* const ru_ones[] = {"ноль","один","два","три","четыре","пять","шесть","семь","восемь","девять","десять","одиннадцать","двенадцать","тринадцать","четырнадцать","пятнадцать","шестнадцать","семнадцать","восемнадцать","девятнадцать"};
const char* const ru_tens[] = {"","","двадцать","тридцать","сорок","пятьдесят","шестьдесят","семьдесят","восемьдесят","девяносто"};
const char* const ru_hundreds[] = {"","сто","двести","триста","четыреста","пятьсот","шестьсот","семьсот","восемьсот","девятьсот"};
std::string ru_under_1000(int n) {
  std::string result;
  if (n >= 100) { result += ru_hundreds[n / 100]; n %= 100; if (n) result += " "; }
  if (n < 20) { if (n) result += ru_ones[n]; }
  else { result += ru_tens[n / 10]; if (n % 10) result += " " + std::string(ru_ones[n % 10]); }
  return result.empty() ? "ноль" : result;
}
std::string ru_number(long long n) {
  if (n < 0) return "минус " + ru_number(-n);
  if (n < 1000) return ru_under_1000(static_cast<int>(n));
  if (n < 1000000) {
    const int thousands = static_cast<int>(n / 1000); const int rest = static_cast<int>(n % 1000);
    std::string thousands_word = thousands == 1 ? "одна" : thousands == 2 ? "две" : ru_under_1000(thousands);
    if (thousands % 100 >= 11 && thousands % 100 <= 14) { /* cardinal form is already correct */ }
    else if (thousands % 10 == 1) { const auto at = thousands_word.rfind("один"); if (at != std::string::npos) thousands_word.replace(at, std::string("один").size(), "одна"); }
    else if (thousands % 10 == 2) { const auto at = thousands_word.rfind("два"); if (at != std::string::npos) thousands_word.replace(at, std::string("два").size(), "две"); }
    std::string result = thousands_word;
    result += (thousands % 10 == 1 && thousands % 100 != 11) ? " тысяча" : (thousands % 10 >= 2 && thousands % 10 <= 4 && (thousands % 100 < 10 || thousands % 100 >= 20)) ? " тысячи" : " тысяч";
    if (rest) result += " " + ru_under_1000(rest);
    return result;
  }
  return std::to_string(n);
}
std::string en_number(long long n) {
  static const char* const ones[] = {"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
  static const char* const tens[] = {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
  if (n < 0) return "minus " + en_number(-n);
  if (n < 20) return ones[n];
  if (n < 100) return std::string(tens[n / 10]) + (n % 10 ? " " + std::string(ones[n % 10]) : "");
  if (n < 1000) return std::string(ones[n / 100]) + " hundred" + (n % 100 ? " " + en_number(n % 100) : "");
  if (n < 1000000) return en_number(n / 1000) + " thousand" + (n % 1000 ? " " + en_number(n % 1000) : "");
  return std::to_string(n);
}
std::string number_or_original(const std::string& token, bool russian, std::vector<TextWarning>& warnings) {
  try {
    const auto value = std::stoll(token); if (value < -999999 || value > 999999) { warnings.push_back({WarningCode::UnresolvedNumber, "Number is outside deterministic range"}); return token; }
    return russian ? ru_number(value) : en_number(value);
  } catch (...) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse number"}); return token; }
}
bool try_parse_long(const std::string& token, long long& value) {
  try { std::size_t consumed = 0; value = std::stoll(token, &consumed); return consumed == token.size() && value >= -999999 && value <= 999999; } catch (...) { return false; }
}
std::string ru_form(long long value, const char* one, const char* few, const char* many) {
  const auto n = std::llabs(value) % 100; const auto last = n % 10;
  if (n >= 11 && n <= 19) return many;
  if (last == 1) return one;
  if (last >= 2 && last <= 4) return few;
  return many;
}
std::string ru_feminine_number(long long value) {
  const auto absolute = std::llabs(value); const auto suffix = absolute % 100;
  if (suffix >= 11 && suffix <= 14) return ru_number(value);
  std::string result = ru_number(value);
  if (absolute % 10 == 1) { const auto at = result.rfind("один"); if (at != std::string::npos) result.replace(at, std::string("один").size(), "одна"); }
  else if (absolute % 10 == 2) { const auto at = result.rfind("два"); if (at != std::string::npos) result.replace(at, std::string("два").size(), "две"); }
  return result;
}
std::string ru_ordinal_day(int day) {
  static const char* const ordinal[] = {"", "первое", "второе", "третье", "четвертое", "пятое", "шестое", "седьмое", "восьмое", "девятое", "десятое", "одиннадцатое", "двенадцатое", "тринадцатое", "четырнадцатое", "пятнадцатое", "шестнадцатое", "семнадцатое", "восемнадцатое", "девятнадцатое", "двадцатое", "двадцать первое", "двадцать второе", "двадцать третье", "двадцать четвертое", "двадцать пятое", "двадцать шестое", "двадцать седьмое", "двадцать восьмое", "двадцать девятое", "тридцатое", "тридцать первое"};
  return day >= 1 && day <= 31 ? ordinal[day] : ru_number(day);
}
std::string ru_year_locative(int year) {
  switch (year) {
    case 1999: return "тысяча девятьсот девяносто девятом";
    case 2000: return "двухтысячном";
    case 2012: return "две тысячи двенадцатом";
    case 2026: return "две тысячи двадцать шестом";
    case 2037: return "две тысячи тридцать седьмом";
    default: break;
  }
  if (year < 1000 || year > 9999) return ru_number(year);
  const int thousands = year / 1000; const int rest = year % 1000;
  std::string result = thousands == 1 ? "одна тысяча" : thousands == 2 ? "две тысячи" : ru_number(thousands) + " тысяч";
  if (rest == 0) return result;
  static const char* const year_day[] = {"", "первом", "втором", "третьем", "четвертом", "пятом", "шестом", "седьмом", "восьмом", "девятом", "десятом", "одиннадцатом", "двенадцатом", "тринадцатом", "четырнадцатом", "пятнадцатом", "шестнадцатом", "семнадцатом", "восемнадцатом", "девятнадцатом", "двадцатом", "двадцать первом", "двадцать втором", "двадцать третьем", "двадцать четвертом", "двадцать пятом", "двадцать шестом", "двадцать седьмом", "двадцать восьмом", "двадцать девятом", "тридцатом", "тридцать первом"};
  if (rest >= 1 && rest <= 31) return result + " " + year_day[rest];
  if (rest < 100) return result + " " + ru_number(rest);
  return result + " " + ru_under_1000(rest);
}
std::string ru_year_genitive(int year) {
  switch (year) {
    case 1999: return "тысяча девятьсот девяносто девятого";
    case 2000: return "двухтысячного";
    case 2012: return "две тысячи двенадцатого";
    case 2026: return "две тысячи двадцать шестого";
    case 2037: return "две тысячи тридцать седьмого";
    default: return ru_number(year);
  }
}
std::string ru_feminine_number(long long value);
std::string ru_decimal(long long integer, const std::string& fraction) {
  const auto denominator = fraction.size() == 1 ? "десятая" : fraction.size() == 2 ? "сотая" : "тысячная";
  const auto denominator_plural = fraction.size() == 1 ? "десятых" : fraction.size() == 2 ? "сотых" : "тысячных";
  const auto fractional = std::stoll(fraction);
  return ru_feminine_number(integer) + (std::llabs(integer) % 10 == 1 && std::llabs(integer) % 100 != 11 ? " целая " : " целых ") + ru_feminine_number(fractional) + (fractional == 1 ? " " + std::string(denominator) : " " + std::string(denominator_plural));
}
std::string en_digits(const std::string& digits) {
  static const char* const names[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"}; std::string result;
  for (const char digit : digits) { if (!result.empty()) result += ' '; result += names[digit - '0']; } return result;
}

struct ProtectedSpan { std::string marker; std::string value; };
std::string marker_for(const std::string& text, std::size_t index) {
  std::string marker = "\x01tts_front_protected_" + std::string(1, static_cast<char>('A' + (index % 26))) + "\x02";
  while (text.find(marker) != std::string::npos) marker.insert(marker.size() - 1, "x");
  return marker;
}
std::string protect_technical(std::string text, std::vector<ProtectedSpan>& protected_spans) {
  const std::vector<std::regex> patterns = {
    std::regex(R"(https?://[^\s]+)"), std::regex(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"),
    std::regex(R"(\b\d{1,3}(?:\.\d{1,3}){3}\b)"), std::regex(R"(\bv\d+(?:\.\d+)+\b)"),
    std::regex(R"(\bHTTP/\d+(?:\.\d+)?\b)"), std::regex(R"(\b(?:RTX|CUDA|GPU|API)\s+\d+(?:\.\d+)?\b)"),
    std::regex(R"(C\+\+)"), std::regex(R"(C#)")
  };
  for (const auto& pattern : patterns) {
    std::string output; std::size_t cursor = 0;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
      const auto begin = static_cast<std::size_t>(it->position()); const auto finish = begin + static_cast<std::size_t>(it->length());
      output.append(text, cursor, begin - cursor); const auto marker = marker_for(text, protected_spans.size());
      protected_spans.push_back({marker, it->str()}); output += marker; cursor = finish;
    }
    output += text.substr(cursor); text = std::move(output);
  }
  return text;
}
std::string restore_technical(std::string text, const std::vector<ProtectedSpan>& protected_spans) {
  for (const auto& span : protected_spans) { std::size_t at = text.find(span.marker); while (at != std::string::npos) { text.replace(at, span.marker.size(), span.value); at = text.find(span.marker, at + span.value.size()); } }
  return text;
}

std::string normalize_ru(std::string text, std::vector<TextWarning>& warnings) {
  std::vector<ProtectedSpan> protected_spans; text = protect_technical(std::move(text), protected_spans);
  text = std::regex_replace(text, std::regex(R"(\b(\d{1,3})\s+(\d{3})\b)"), "$1$2");
  text = replace_matches(std::move(text), std::regex(R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)"), [&](const std::smatch& match) { long long day = 0, month = 0, year = 0; if (!try_parse_long(match[1].str(), day) || !try_parse_long(match[2].str(), month) || !try_parse_long(match[3].str(), year)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian date"}); return match.str(); } static const char* const months[] = {"", "января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"}; return ru_ordinal_day(static_cast<int>(day)) + " " + (month >= 1 && month <= 12 ? months[month] : match[2].str()) + " " + ru_year_genitive(static_cast<int>(year)) + " года"; });
  text = replace_matches(std::move(text), std::regex(R"(\b(\d{4})\s*г\.)"), [&](const std::smatch& match) { long long year = 0; if (!try_parse_long(match[1].str(), year)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian year"}); return match.str(); } return ru_year_locative(static_cast<int>(year)) + " году"; });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+),([0-9]+)\s*%)"), [&](const std::smatch& match) { long long integer = 0, fraction = 0; if (!try_parse_long(match[1].str(), integer) || !try_parse_long(match[2].str(), fraction)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian decimal percent"}); return match.str(); } return ru_decimal(integer, match[2].str()) + " процентов"; });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+)\s*%)"), [&](const std::smatch& match) { long long n = 0; if (!try_parse_long(match[1].str(), n)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian percent"}); return match.str(); } return ru_number(n) + " " + ru_form(n, "процент", "процента", "процентов"); });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+)\s*(рублей|рубля|рубль|руб\.?)([^А-Яа-яЁёA-Za-z0-9]|$))"), [&](const std::smatch& match) { long long n = 0; if (!try_parse_long(match[1].str(), n)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian currency"}); return match.str(); } return ru_number(n) + " " + ru_form(n, "рубль", "рубля", "рублей") + match[3].str(); });
  text = replace_matches(std::move(text), std::regex(R"(\b(\d{1,2}):(\d{2})\b)"), [&](const std::smatch& match) { long long h = 0, m = 0; if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian time"}); return match.str(); } return ru_number(h) + " " + ru_form(h, "час", "часа", "часов") + " " + ru_feminine_number(m) + " " + ru_form(m, "минута", "минуты", "минут"); });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+)[,](\d+))"), [&](const std::smatch& match) { long long integer = 0, fraction = 0; if (!try_parse_long(match[1].str(), integer) || !try_parse_long(match[2].str(), fraction)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian decimal"}); return match.str(); } return ru_decimal(integer, match[2].str()); });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+)\s*(километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м)([^А-Яа-яЁёA-Za-z0-9]|$))"), [&](const std::smatch& match) { long long n = 0; if (!try_parse_long(match[1].str(), n)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse Russian measurement"}); return match.str(); } const auto source = match[2].str(); const std::string unit = source.find("кг") == 0 || source.find("килограмм") == 0 ? ru_form(n, "килограмм", "килограмма", "килограммов") : source.find("км") == 0 || source.find("километр") == 0 ? ru_form(n, "километр", "километра", "километров") : source.find("см") == 0 || source.find("сантиметр") == 0 ? ru_form(n, "сантиметр", "сантиметра", "сантиметров") : source.find("мм") == 0 || source.find("миллиметр") == 0 ? ru_form(n, "миллиметр", "миллиметра", "миллиметров") : source == "м" ? ru_form(n, "метр", "метра", "метров") : source == "ГБ" ? ru_form(n, "гигабайт", "гигабайта", "гигабайт") : ru_form(n, "мегабайт", "мегабайта", "мегабайт"); return ru_number(n) + " " + unit + match[3].str(); });
  text = replace_matches(std::move(text), std::regex(R"(-?\d+)"), [&](const std::smatch& match) { return number_or_original(match.str(), true, warnings); });
  text = std::regex_replace(text, std::regex(R"(\bт\.д\.)"), "так далее");
  text = std::regex_replace(text, std::regex(R"(\bт\.п\.)"), "тому подобное");
  return restore_technical(std::move(text), protected_spans);
}

std::string normalize_en(std::string text, std::vector<TextWarning>& warnings) {
  std::vector<ProtectedSpan> protected_spans; text = protect_technical(std::move(text), protected_spans);
  text = std::regex_replace(text, std::regex(R"(\b(\d{1,3})\s+(\d{3})\b)"), "$1$2");
  text = replace_matches(std::move(text), std::regex(R"(\$([0-9]+)\.([0-9]{1,}))"), [&](const std::smatch& match) { long long dollars = 0, cents = 0; if (!try_parse_long(match[1].str(), dollars) || !try_parse_long(match[2].str(), cents)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English currency"}); return match.str(); } return en_number(dollars) + (dollars == 1 ? " dollar" : " dollars") + " " + en_digits(match[2].str()) + " cents"; });
  text = replace_matches(std::move(text), std::regex(R"(\$([0-9]+))"), [&](const std::smatch& match) { long long dollars = 0; if (!try_parse_long(match[1].str(), dollars)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English currency"}); return match.str(); } return en_number(dollars) + (dollars == 1 ? " dollar" : " dollars"); });
  text = replace_matches(std::move(text), std::regex(R"((-?[0-9]+(?:\.[0-9]+)?)\s*%)"), [&](const std::smatch& match) { const auto value = match[1].str(); const auto dot = value.find('.'); long long integer = 0; if (!try_parse_long(dot == std::string::npos ? value : value.substr(0, dot), integer)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English percent"}); return match.str(); } return (dot == std::string::npos ? en_number(integer) : en_number(integer) + " point " + en_digits(value.substr(dot + 1))) + " percent"; });
  text = replace_matches(std::move(text), std::regex(R"(\b(\d{1,2}):(\d{2})\b)"), [&](const std::smatch& match) { long long h = 0, m = 0; if (!try_parse_long(match[1].str(), h) || !try_parse_long(match[2].str(), m)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English time"}); return match.str(); } return en_number(h) + (h == 1 ? " hour " : " hours ") + en_number(m) + (m == 1 ? " minute" : " minutes"); });
  text = replace_matches(std::move(text), std::regex(R"(-?\d+\.\d+)"), [&](const std::smatch& match) { const auto dot = match.str().find('.'); long long integer = 0; if (!try_parse_long(match.str().substr(0, dot), integer)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English decimal"}); return match.str(); } return en_number(integer) + " point " + en_digits(match.str().substr(dot + 1)); });
  text = replace_matches(std::move(text), std::regex(R"((-?\d+)\s*(kilometers|kilometres|km|kilograms|kg|meters|metres|m|centimeters|centimetres|cm|millimeters|millimetres|mm|GB|MB)([^A-Za-z0-9]|$))"), [&](const std::smatch& match) { long long n = 0; if (!try_parse_long(match[1].str(), n)) { warnings.push_back({WarningCode::UnresolvedNumber, "Unable to parse English measurement"}); return match.str(); } const auto unit = match[2].str(); const bool singular = n == 1; const std::string spoken = unit == "kg" || unit.find("kilogram") == 0 ? (singular ? "kilogram" : "kilograms") : unit == "km" || unit.find("kilomet") == 0 ? (singular ? "kilometer" : "kilometers") : unit == "m" || unit == "meters" || unit == "metres" ? (singular ? "meter" : "meters") : unit == "cm" || unit.find("centimet") == 0 ? (singular ? "centimeter" : "centimeters") : unit == "mm" || unit.find("millimet") == 0 ? (singular ? "millimeter" : "millimeters") : (singular ? "gigabyte" : "gigabytes"); return en_number(n) + " " + spoken + match[3].str(); });
  text = replace_matches(std::move(text), std::regex(R"(-?\d+)"), [&](const std::smatch& match) { return number_or_original(match.str(), false, warnings); });
  return restore_technical(std::move(text), protected_spans);
}

struct Span { std::size_t begin = 0; std::size_t end = 0; };
std::vector<Span> token_spans(const std::string& text) {
  std::vector<CodePoint> points; decode_utf8(text, points); std::vector<Span> spans; std::size_t begin = std::string::npos; std::size_t end = 0;
  for (const auto& point : points) {
    if (is_word_codepoint(point.value)) { if (begin == std::string::npos) begin = point.offset; end = point.offset + point.length; }
    else if (begin != std::string::npos) { spans.push_back({begin, end}); begin = std::string::npos; }
  }
  if (begin != std::string::npos) spans.push_back({begin, end});
  return spans;
}
Language detect_language(std::string_view text, bool& has_cyrillic, bool& has_latin) {
  std::vector<CodePoint> points; decode_utf8(text, points); has_cyrillic = std::any_of(points.begin(), points.end(), [](const CodePoint& p) { return is_cyrillic(p.value); }); has_latin = std::any_of(points.begin(), points.end(), [](const CodePoint& p) { return is_latin(p.value); });
  return has_cyrillic ? Language::Russian : Language::English;
}

} // namespace

bool TextFrontendResult::has_uncertainty() const noexcept { return std::any_of(warnings.begin(), warnings.end(), [](const TextWarning& warning) { return warning.code == WarningCode::AmbiguousNormalization || warning.code == WarningCode::UnresolvedNumber; }); }

TextFrontendResult TextFrontend::process(std::string_view input, const TextFrontendOptions& options) const {
  TextFrontendResult result; result.original_text = std::string(input); std::vector<CodePoint> points;
  if (!decode_utf8(input, points)) { result.warnings.push_back({WarningCode::InvalidUtf8, "Input is not valid UTF-8"}); return result; }
  std::string text(input); if (options.cleanup_unicode) text = cleanup_text(std::move(text));
  bool has_cyrillic = false; bool has_latin = false; Language language = options.language;
  if (language == Language::Auto) language = detect_language(text, has_cyrillic, has_latin);
  if (options.language == Language::Auto && has_cyrillic && has_latin) result.warnings.push_back({WarningCode::AmbiguousNormalization, "Mixed Cyrillic/Latin input uses Russian normalization by policy"});
  if (language != Language::Russian && language != Language::English) { result.warnings.push_back({WarningCode::UnsupportedLanguage, "Unsupported language"}); return result; }
  if (options.normalize) text = language == Language::Russian ? normalize_ru(std::move(text), result.warnings) : normalize_en(std::move(text), result.warnings);
  if (options.cleanup_unicode) text = cleanup_text(std::move(text));
  result.normalized_text = text; result.pronunciation_text = text;

  // Dictionary phrases are matched on complete token sequences, longest first, without rescanning output.
  if (options.apply_dictionary && options.dictionary) {
    const auto spans = token_spans(text);
    std::string rendered; std::size_t cursor = 0;
    for (std::size_t i = 0; i < spans.size();) {
      const PronunciationDictionary::Entry* best = nullptr; std::size_t best_end = i;
      for (const auto& entry : options.dictionary->entries()) {
        if (entry.match != PronunciationDictionary::Match::ExactPhrase) continue;
        const auto phrase_spans = token_spans(entry.pattern); if (phrase_spans.empty() || i + phrase_spans.size() > spans.size()) continue;
        bool match = true;
        for (std::size_t j = 0; j < phrase_spans.size(); ++j) {
          if (text.substr(spans[i + j].begin, spans[i + j].end - spans[i + j].begin) != entry.pattern.substr(phrase_spans[j].begin, phrase_spans[j].end - phrase_spans[j].begin)) { match = false; break; }
        }
        if (match && (!best || phrase_spans.size() > best_end - i)) { best = &entry; best_end = i + phrase_spans.size(); }
      }
      const auto token = text.substr(spans[i].begin, spans[i].end - spans[i].begin);
      if (best) {
        rendered.append(text, cursor, spans[i].begin - cursor); rendered += best->pronunciation;
        const auto phrase_surface = text.substr(spans[i].begin, spans[best_end - 1].end - spans[i].begin);
        result.dictionary_replacements.push_back({phrase_surface, best->pronunciation, spans[i].begin});
        if (options.resolve_stress && options.stress_mode != StressMode::Disabled && best->stressed_vowel) result.stress_decisions.push_back({phrase_surface, best->stressed_vowel, true, "pronunciation dictionary phrase"});
        cursor = spans[best_end - 1].end; i = best_end; continue;
      }
      rendered.append(text, cursor, spans[i].begin - cursor);
      if (const auto* entry = options.dictionary->find_token(token)) {
        rendered += entry->pronunciation;
        if (entry->pronunciation != token) result.dictionary_replacements.push_back({token, entry->pronunciation, spans[i].begin});
      } else rendered += token;
      cursor = spans[i].end; ++i;
    }
    rendered += text.substr(cursor); result.pronunciation_text = std::move(rendered);
  }

  for (const auto& span : token_spans(result.normalized_text)) {
    const std::string token = result.normalized_text.substr(span.begin, span.end - span.begin); WordPronunciation word; word.surface = token; word.source_offset = span.begin;
    for (std::size_t replacement_index = 0; replacement_index < result.dictionary_replacements.size(); ++replacement_index) {
      const auto& replacement = result.dictionary_replacements[replacement_index]; const auto replacement_end = replacement.offset + replacement.input.size();
      if (span.begin >= replacement.offset && span.end <= replacement_end) { word.dictionary_replacement = replacement_index; word.from_dictionary = true; if (span.begin == replacement.offset) word.pronunciation = replacement.output; break; }
    }
    if (options.dictionary && options.apply_dictionary) { if (const auto* entry = options.dictionary->find_token(token)) { word.from_dictionary = true; word.pronunciation = entry->pronunciation; if (options.resolve_stress && options.stress_mode != StressMode::Disabled) word.stressed_vowel = entry->stressed_vowel; } }
    result.words.push_back(word); if (options.diagnostics || word.from_dictionary) result.stress_decisions.push_back({token, word.stressed_vowel, word.from_dictionary, word.from_dictionary ? "pronunciation dictionary" : "no deterministic stress rule"});
  }
  if (options.resolve_stress && options.stress_mode == StressMode::Automatic) result.warnings.push_back({WarningCode::AutomaticStressUnavailable, "Automatic stress is not available until a parity-proven backend is added"});
  return result;
}

const char* to_string(Language language) noexcept { switch (language) { case Language::Auto: return "auto"; case Language::Russian: return "russian"; case Language::English: return "english"; } return "unknown"; }
const char* to_string(WarningCode code) noexcept { switch (code) { case WarningCode::InvalidUtf8: return "invalid_utf8"; case WarningCode::UnsupportedLanguage: return "unsupported_language"; case WarningCode::AmbiguousNormalization: return "ambiguous_normalization"; case WarningCode::UnresolvedNumber: return "unresolved_number"; case WarningCode::AutomaticStressUnavailable: return "automatic_stress_unavailable"; case WarningCode::DictionaryParseError: return "dictionary_parse_error"; } return "unknown"; }
} // namespace tts_front
