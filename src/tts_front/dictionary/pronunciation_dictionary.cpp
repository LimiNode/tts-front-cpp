#include "tts_front.hpp"
#include "tts_front/core/utf8.hpp"
#include "tts_front/serialization/json.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>

namespace tts_front {
namespace {

std::size_t count_vowels(std::string_view text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        const std::size_t length = detail::utf8_sequence_length(c);
        if (length == 0 || i + length > text.size())
            break;
        const std::string_view unit = text.substr(i, length);
        if (unit == "a" || unit == "e" || unit == "i" || unit == "o" || unit == "u" ||
            unit == "y" || unit == "A" || unit == "E" || unit == "I" || unit == "O" ||
            unit == "U" || unit == "Y" || unit == "а" || unit == "е" || unit == "ё" ||
            unit == "и" || unit == "о" || unit == "у" || unit == "ы" || unit == "э" ||
            unit == "ю" || unit == "я" || unit == "А" || unit == "Е" || unit == "Ё" ||
            unit == "И" || unit == "О" || unit == "У" || unit == "Ы" || unit == "Э" ||
            unit == "Ю" || unit == "Я")
            ++count;
        i += length;
    }
    return count;
}
std::string lower_unicode_ru_en(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        const auto c = static_cast<unsigned char>(value[i]);
        const std::size_t length = detail::utf8_sequence_length(c);
        if (length == 0 || i + length > value.size()) {
            output.push_back(value[i++]);
            continue;
        }
        if (length == 1)
            output.push_back(static_cast<char>(std::tolower(c)));
        else if (length == 2 && static_cast<unsigned char>(value[i]) == 0xd0 &&
                 static_cast<unsigned char>(value[i + 1]) >= 0x90 &&
                 static_cast<unsigned char>(value[i + 1]) <= 0x9f) {
            output.push_back(static_cast<char>(0xd0));
            output.push_back(static_cast<char>(static_cast<unsigned char>(value[i + 1]) + 0x20));
        } else if (length == 2 && static_cast<unsigned char>(value[i]) == 0xd0 &&
                   static_cast<unsigned char>(value[i + 1]) >= 0xa0 &&
                   static_cast<unsigned char>(value[i + 1]) <= 0xaf) {
            output.push_back(static_cast<char>(0xd1));
            output.push_back(static_cast<char>(static_cast<unsigned char>(value[i + 1]) - 0x20));
        } else if (length == 2 && static_cast<unsigned char>(value[i]) == 0xd0 &&
                   static_cast<unsigned char>(value[i + 1]) == 0x81) {
            output += "ё";
        } else
            output.append(value, i, length);
        i += length;
    }
    return output;
}

std::string dictionary_entry_key(const PronunciationDictionary::Entry& entry) {
    return entry.match == PronunciationDictionary::Match::CaseInsensitiveToken
               ? lower_unicode_ru_en(entry.pattern)
               : entry.pattern;
}

bool same_dictionary_key(const PronunciationDictionary::Entry& lhs,
                         const PronunciationDictionary::Entry& rhs) {
    return lhs.match == rhs.match && dictionary_entry_key(lhs) == dictionary_entry_key(rhs);
}

bool parse_dictionary_json(std::string_view source,
                           std::vector<PronunciationDictionary::Entry>& entries,
                           std::string& error) {
    try {
        const auto root = detail::parse_json(source);
        if (root.kind != detail::JsonValue::Kind::Array)
            throw std::runtime_error("top-level JSON value must be an array");
        for (const auto& value : root.array) {
            if (value.kind != detail::JsonValue::Kind::Object)
                throw std::runtime_error("dictionary entry must be an object");
            PronunciationDictionary::Entry entry;
            bool has_pattern = false;
            bool has_pronunciation = false;
            bool force_phrase = false;
            for (const auto& [key, field] : value.object) {
                if (key == "pattern" || key == "token" || key == "phrase") {
                    if (has_pattern || field.kind != detail::JsonValue::Kind::String)
                        throw std::runtime_error("entry pattern must be a single string");
                    entry.pattern = field.string;
                    has_pattern = true;
                    force_phrase = key == "phrase";
                } else if (key == "pronunciation" || key == "replacement") {
                    if (has_pronunciation || field.kind != detail::JsonValue::Kind::String)
                        throw std::runtime_error("entry pronunciation must be a single string");
                    entry.pronunciation = field.string;
                    has_pronunciation = true;
                } else if (key == "match") {
                    if (field.kind != detail::JsonValue::Kind::String)
                        throw std::runtime_error("entry match must be a string");
                    if (field.string == "exact_token" || field.string == "case_sensitive")
                        entry.match = PronunciationDictionary::Match::ExactToken;
                    else if (field.string == "case_insensitive")
                        entry.match = PronunciationDictionary::Match::CaseInsensitiveToken;
                    else if (field.string == "phrase")
                        entry.match = PronunciationDictionary::Match::ExactPhrase;
                    else
                        throw std::runtime_error("unknown match value");
                } else if (key == "stressed_vowel") {
                    if (field.kind != detail::JsonValue::Kind::Number || field.string.empty() ||
                        field.string.find_first_not_of("0123456789") != std::string::npos)
                        throw std::runtime_error("stressed_vowel must be an unsigned integer");
                    try {
                        const auto value = std::stoull(field.string);
                        if (value > std::numeric_limits<std::size_t>::max())
                            throw std::out_of_range("stressed_vowel");
                        entry.stressed_vowel = static_cast<std::size_t>(value);
                    } catch (...) {
                        throw std::runtime_error("invalid stressed_vowel");
                    }
                } else {
                    throw std::runtime_error("unknown entry field: " + key);
                }
            }
            if (force_phrase)
                entry.match = PronunciationDictionary::Match::ExactPhrase;
            if (!has_pattern || !has_pronunciation || entry.pattern.empty() ||
                entry.pronunciation.empty())
                throw std::runtime_error("entry requires non-empty pattern and pronunciation");
            if (entry.stressed_vowel && *entry.stressed_vowel >= count_vowels(entry.pronunciation))
                throw std::runtime_error("stressed_vowel is outside pronunciation vowel range");
            entries.push_back(std::move(entry));
        }
        return true;
    } catch (const std::runtime_error& exception) {
        error = exception.what();
        return false;
    }
}
} // namespace

bool PronunciationDictionary::add_entry(Entry entry) {
    if (entry.pattern.empty() || entry.pronunciation.empty())
        return false;
    if (!detail::is_valid_utf8(entry.pattern) || !detail::is_valid_utf8(entry.pronunciation))
        return false;
    if (entry.stressed_vowel && *entry.stressed_vowel >= count_vowels(entry.pronunciation))
        return false;
    for (const auto& existing : m_entries)
        if (same_dictionary_key(existing, entry))
            return false;
    m_entries.push_back(std::move(entry));
    return true;
}
bool PronunciationDictionary::add_token(std::string token,
                                        std::string pronunciation,
                                        std::optional<std::size_t> stress) {
    return add_entry({std::move(token), std::move(pronunciation), Match::ExactToken, stress});
}
bool PronunciationDictionary::add_case_insensitive_token(std::string token,
                                                         std::string pronunciation,
                                                         std::optional<std::size_t> stress) {
    return add_entry(
        {std::move(token), std::move(pronunciation), Match::CaseInsensitiveToken, stress});
}
bool PronunciationDictionary::add_phrase(std::string phrase,
                                         std::string pronunciation,
                                         std::optional<std::size_t> stress) {
    return add_entry({std::move(phrase), std::move(pronunciation), Match::ExactPhrase, stress});
}
const PronunciationDictionary::Entry*
PronunciationDictionary::find_token(std::string_view token) const {
    for (const auto& entry : m_entries) {
        if (entry.match == Match::ExactPhrase)
            continue;
        if (entry.match == Match::CaseInsensitiveToken
                ? lower_unicode_ru_en(entry.pattern) == lower_unicode_ru_en(token)
                : entry.pattern == token)
            return &entry;
    }
    return nullptr;
}
bool PronunciationDictionary::load_file(const std::string& path,
                                        std::vector<TextWarning>* warnings) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (warnings)
            warnings->push_back({WarningCode::DictionaryParseError,
                                 "Cannot open pronunciation dictionary: " + path});
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();
    if (!detail::is_valid_utf8(json)) {
        if (warnings)
            warnings->push_back(
                {WarningCode::DictionaryParseError, "Dictionary file is not valid UTF-8"});
        return false;
    }
    std::vector<Entry> parsed;
    std::string error;
    if (!parse_dictionary_json(json, parsed, error)) {
        if (warnings)
            warnings->push_back(
                {WarningCode::DictionaryParseError, "Invalid pronunciation dictionary: " + error});
        return false;
    }
    for (std::size_t i = 0; i < parsed.size(); ++i)
        for (std::size_t j = i + 1; j < parsed.size(); ++j)
            if (same_dictionary_key(parsed[i], parsed[j])) {
                if (warnings)
                    warnings->push_back({WarningCode::DictionaryParseError,
                                         "Duplicate pronunciation dictionary entry"});
                return false;
            }
    for (const auto& candidate : parsed)
        for (const auto& existing : m_entries)
            if (same_dictionary_key(candidate, existing)) {
                if (warnings)
                    warnings->push_back({WarningCode::DictionaryParseError,
                                         "Duplicate pronunciation dictionary entry"});
                return false;
            }
    m_entries.insert(m_entries.end(), parsed.begin(), parsed.end());
    return true;
}
} // namespace tts_front
