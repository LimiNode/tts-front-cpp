#include "detail/utf8.hpp"
#include "tts_front/tts_front.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace tts_front {
namespace {

std::size_t count_vowels(std::string_view text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        const std::size_t length = detail::utf8_sequence_length(c);
        if (length == 0 || i + length > text.size())
            break;
        const std::string unit(text.substr(i, length));
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
std::string lower_unicode_ru_en(std::string value) {
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

class JsonParser {
  public:
    explicit JsonParser(std::string_view source) : m_source(source) {}
    bool parse(std::vector<PronunciationDictionary::Entry>& entries, std::string& error) {
        skip_space();
        if (!consume('['))
            return fail(error, "top-level JSON value must be an array");
        skip_space();
        if (consume(']')) {
            skip_space();
            return m_position == m_source.size() ? true
                                                 : fail(error, "trailing data after JSON array");
        }
        while (true) {
            PronunciationDictionary::Entry entry;
            bool has_pattern = false;
            bool has_pronunciation = false;
            if (!parse_entry(entry, has_pattern, has_pronunciation, error))
                return false;
            if (!has_pattern || !has_pronunciation || entry.pattern.empty() ||
                entry.pronunciation.empty())
                return fail(error, "entry requires non-empty pattern and pronunciation");
            if (entry.stressed_vowel && *entry.stressed_vowel >= count_vowels(entry.pronunciation))
                return fail(error, "stressed_vowel is outside pronunciation vowel range");
            entries.push_back(std::move(entry));
            skip_space();
            if (consume(']')) {
                skip_space();
                return m_position == m_source.size()
                           ? true
                           : fail(error, "trailing data after JSON array");
            }
            if (!consume(','))
                return fail(error, "expected comma between entries");
        }
    }

  private:
    bool fail(std::string& error, std::string message) {
        error = std::move(message) + " at byte " + std::to_string(m_position);
        return false;
    }
    void skip_space() {
        while (m_position < m_source.size() &&
               std::isspace(static_cast<unsigned char>(m_source[m_position])))
            ++m_position;
    }
    bool consume(char expected) {
        skip_space();
        if (m_position >= m_source.size() || m_source[m_position] != expected)
            return false;
        ++m_position;
        return true;
    }
    bool parse_entry(PronunciationDictionary::Entry& entry,
                     bool& has_pattern,
                     bool& has_pronunciation,
                     std::string& error) {
        if (!consume('{'))
            return fail(error, "expected object");
        bool force_phrase = false;
        skip_space();
        if (consume('}'))
            return fail(error, "empty entry");
        std::unordered_set<std::string> keys;
        while (true) {
            std::string key;
            if (!parse_string(key, error) || !consume(':'))
                return false;
            if (!keys.insert(key).second)
                return fail(error, "duplicate object key");
            if (key == "pattern" || key == "token" || key == "phrase") {
                if (!parse_string(entry.pattern, error))
                    return false;
                has_pattern = true;
                force_phrase = force_phrase || key == "phrase";
            } else if (key == "pronunciation" || key == "replacement") {
                if (!parse_string(entry.pronunciation, error))
                    return false;
                has_pronunciation = true;
            } else if (key == "match") {
                std::string match;
                if (!parse_string(match, error))
                    return false;
                if (match == "exact_token" || match == "case_sensitive")
                    entry.match = PronunciationDictionary::Match::ExactToken;
                else if (match == "case_insensitive")
                    entry.match = PronunciationDictionary::Match::CaseInsensitiveToken;
                else if (match == "phrase")
                    entry.match = PronunciationDictionary::Match::ExactPhrase;
                else
                    return fail(error, "unknown match value");
            } else if (key == "stressed_vowel") {
                std::size_t value = 0;
                if (!parse_unsigned(value, error))
                    return false;
                entry.stressed_vowel = value;
            } else
                return fail(error, "unknown entry field");
            skip_space();
            if (consume('}')) {
                if (force_phrase)
                    entry.match = PronunciationDictionary::Match::ExactPhrase;
                return true;
            }
            if (!consume(','))
                return fail(error, "expected comma in entry");
        }
    }
    bool parse_unsigned(std::size_t& value, std::string& error) {
        skip_space();
        const auto begin = m_position;
        while (m_position < m_source.size() &&
               std::isdigit(static_cast<unsigned char>(m_source[m_position])))
            ++m_position;
        if (begin == m_position)
            return fail(error, "expected unsigned integer");
        try {
            value = static_cast<std::size_t>(
                std::stoull(std::string(m_source.substr(begin, m_position - begin))));
        } catch (...) {
            return fail(error, "invalid unsigned integer");
        }
        return true;
    }
    bool parse_hex4(unsigned& value, std::string& error) {
        if (m_position + 4 > m_source.size())
            return fail(error, "short unicode escape");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = m_source[m_position++];
            value <<= 4;
            if (h >= '0' && h <= '9')
                value += static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f')
                value += static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
                value += static_cast<unsigned>(h - 'A' + 10);
            else
                return fail(error, "invalid unicode escape");
        }
        return true;
    }
    static void append_codepoint(std::string& output, unsigned value) {
        if (value < 0x80)
            output.push_back(static_cast<char>(value));
        else if (value < 0x800) {
            output.push_back(static_cast<char>(0xc0 | (value >> 6)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else if (value < 0x10000) {
            output.push_back(static_cast<char>(0xe0 | (value >> 12)));
            output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (value >> 18)));
            output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
    }
    bool parse_string(std::string& output, std::string& error) {
        if (!consume('"'))
            return fail(error, "expected string");
        output.clear();
        while (m_position < m_source.size()) {
            const char c = m_source[m_position++];
            if (c == '"')
                return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return fail(error, "control character in JSON string");
            if (c != '\\') {
                output.push_back(c);
                continue;
            }
            if (m_position >= m_source.size())
                return fail(error, "unterminated escape");
            const char escaped = m_source[m_position++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                output.push_back(escaped);
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u': {
                unsigned value = 0;
                if (!parse_hex4(value, error))
                    return false;
                if (value >= 0xd800 && value <= 0xdbff) {
                    if (m_position + 6 > m_source.size() || m_source[m_position] != '\\' ||
                        m_source[m_position + 1] != 'u')
                        return fail(error, "high surrogate without low surrogate");
                    m_position += 2;
                    unsigned low = 0;
                    if (!parse_hex4(low, error) || low < 0xdc00 || low > 0xdfff)
                        return fail(error, "invalid low surrogate");
                    value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
                } else if (value >= 0xdc00 && value <= 0xdfff)
                    return fail(error, "unexpected low surrogate");
                append_codepoint(output, value);
                break;
            }
            default:
                return fail(error, "unknown escape");
            }
        }
        return fail(error, "unterminated string");
    }
    std::string_view m_source;
    std::size_t m_position = 0;
};
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
                ? lower_unicode_ru_en(entry.pattern) == lower_unicode_ru_en(std::string(token))
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
    JsonParser parser(json);
    if (!parser.parse(parsed, error)) {
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
