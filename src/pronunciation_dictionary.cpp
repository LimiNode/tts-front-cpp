#include "tts_front/tts_front.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace tts_front {
namespace {
std::string ascii_lower(std::string value) {
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
std::string unescape(std::string value) {
  std::string out;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i + 1 < value.size()) {
      const char n = value[++i];
      out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
    } else out += value[i];
  }
  return out;
}
}

void PronunciationDictionary::add_entry(Entry entry) { m_entries.push_back(std::move(entry)); }
void PronunciationDictionary::add_token(std::string token, std::string pronunciation,
                                        std::optional<std::size_t> stress) {
  add_entry({std::move(token), std::move(pronunciation), Match::ExactToken, stress});
}
void PronunciationDictionary::add_case_insensitive_token(std::string token, std::string pronunciation,
                                                         std::optional<std::size_t> stress) {
  add_entry({std::move(token), std::move(pronunciation), Match::CaseInsensitiveToken, stress});
}
void PronunciationDictionary::add_phrase(std::string phrase, std::string pronunciation,
                                         std::optional<std::size_t> stress) {
  add_entry({std::move(phrase), std::move(pronunciation), Match::ExactPhrase, stress});
}

const PronunciationDictionary::Entry* PronunciationDictionary::find_token(std::string_view token) const noexcept {
  for (const auto& entry : m_entries) {
    if (entry.match == Match::ExactPhrase) continue;
    if (entry.match == Match::CaseInsensitiveToken) {
      if (ascii_lower(entry.pattern) == ascii_lower(std::string(token))) return &entry;
    } else if (entry.pattern == token) return &entry;
  }
  return nullptr;
}

bool PronunciationDictionary::load_file(const std::string& path, std::vector<TextWarning>* warnings) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (warnings) warnings->push_back({WarningCode::DictionaryParseError, "Cannot open pronunciation dictionary: " + path});
    return false;
  }
  std::ostringstream buffer; buffer << file.rdbuf();
  const std::string text = buffer.str();
  // Deliberately small, dependency-free JSON reader for the documented entry format.
  const std::regex object_re(R"(\{[^\}]*\})");
  const std::regex field_re(R"REGEX("([A-Za-z_]+)"\s*:\s*"((\\.|[^"\\])*)")REGEX");
  const std::regex stress_re(R"("stressed_vowel"\s*:\s*(\d+))");
  bool loaded = false;
  for (std::sregex_iterator it(text.begin(), text.end(), object_re), end; it != end; ++it) {
    Entry entry; std::string match; const std::string object = it->str();
    for (std::sregex_iterator f(object.begin(), object.end(), field_re), fend; f != fend; ++f) {
      const auto key = (*f)[1].str(); const auto value = unescape((*f)[2].str());
      if (key == "pattern" || key == "token" || key == "phrase") entry.pattern = value;
      else if (key == "pronunciation" || key == "replacement") entry.pronunciation = value;
      else if (key == "match") match = value;
    }
    std::smatch stress_match;
    if (std::regex_search(object, stress_match, stress_re)) entry.stressed_vowel = static_cast<std::size_t>(std::stoul(stress_match[1].str()));
    if (entry.pattern.empty()) continue;
    if (match == "phrase" || text.substr(static_cast<std::size_t>(it->position()), it->length()).find("\"phrase\"") != std::string::npos)
      entry.match = Match::ExactPhrase;
    else if (match == "case_insensitive" || match == "insensitive") entry.match = Match::CaseInsensitiveToken;
    else if (match == "case_sensitive") entry.match = Match::CaseSensitiveToken;
    add_entry(std::move(entry)); loaded = true;
  }
  if (!loaded && warnings) warnings->push_back({WarningCode::DictionaryParseError, "No dictionary entries found in: " + path});
  return loaded;
}
} // namespace tts_front
