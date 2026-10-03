#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tts_front::detail {

struct JsonValue {
    enum class Kind { Null, Boolean, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
  public:
    explicit JsonParser(std::string_view source) : source_(source) {}

    JsonValue parse() {
        skip_space();
        auto result = parse_value();
        skip_space();
        if (position_ != source_.size())
            fail("trailing data");
        return result;
    }

  private:
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error("malformed JSON at byte " + std::to_string(position_) + ": " +
                                 std::string(message));
    }

    void skip_space() {
        while (position_ < source_.size()) {
            const auto character = source_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
                break;
            ++position_;
        }
    }

    bool consume(char expected) {
        skip_space();
        if (position_ >= source_.size() || source_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    JsonValue parse_value() {
        skip_space();
        if (position_ >= source_.size())
            fail("expected value");
        switch (source_[position_]) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return {JsonValue::Kind::String, false, parse_string(), {}, {}};
        case 't':
            consume_literal("true");
            return {JsonValue::Kind::Boolean, true, {}, {}, {}};
        case 'f':
            consume_literal("false");
            return {JsonValue::Kind::Boolean, false, {}, {}, {}};
        case 'n':
            consume_literal("null");
            return {};
        default:
            if (source_[position_] == '-' ||
                (source_[position_] >= '0' && source_[position_] <= '9'))
                return parse_number();
            fail("unexpected value");
        }
    }

    JsonValue parse_object() {
        consume('{');
        JsonValue result;
        result.kind = JsonValue::Kind::Object;
        skip_space();
        if (consume('}'))
            return result;
        while (true) {
            skip_space();
            if (position_ >= source_.size() || source_[position_] != '"')
                fail("expected object key");
            const auto key = parse_string();
            if (!consume(':'))
                fail("expected object colon");
            if (!result.object.emplace(key, parse_value()).second)
                fail("duplicate object key");
            if (consume('}'))
                return result;
            if (!consume(','))
                fail("expected object comma");
        }
    }

    JsonValue parse_array() {
        consume('[');
        JsonValue result;
        result.kind = JsonValue::Kind::Array;
        skip_space();
        if (consume(']'))
            return result;
        while (true) {
            result.array.push_back(parse_value());
            if (consume(']'))
                return result;
            if (!consume(','))
                fail("expected array comma");
        }
    }

    JsonValue parse_number() {
        const auto begin = position_;
        if (source_[position_] == '-')
            ++position_;
        if (position_ >= source_.size() || source_[position_] < '0' || source_[position_] > '9')
            fail("malformed number");
        if (source_[position_] == '0') {
            ++position_;
        } else {
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9')
                ++position_;
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            const auto fraction = position_;
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9')
                ++position_;
            if (fraction == position_)
                fail("malformed number fraction");
        }
        if (position_ < source_.size() &&
            (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() &&
                (source_[position_] == '+' || source_[position_] == '-'))
                ++position_;
            const auto exponent = position_;
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9')
                ++position_;
            if (exponent == position_)
                fail("malformed number exponent");
        }
        JsonValue result;
        result.kind = JsonValue::Kind::Number;
        result.string = std::string(source_.substr(begin, position_ - begin));
        return result;
    }

    void consume_literal(std::string_view literal) {
        if (source_.substr(position_, literal.size()) != literal)
            fail("malformed literal");
        position_ += literal.size();
    }

    static void append_codepoint(std::string& output, unsigned value) {
        if (value >= 0xd800 && value <= 0xdfff)
            throw std::runtime_error("malformed JSON unicode surrogate");
        if (value < 0x80)
            output.push_back(static_cast<char>(value));
        else if (value < 0x800) {
            output.push_back(static_cast<char>(0xc0 | (value >> 6)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else if (value < 0x10000) {
            output.push_back(static_cast<char>(0xe0 | (value >> 12)));
            output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else if (value <= 0x10ffff) {
            output.push_back(static_cast<char>(0xf0 | (value >> 18)));
            output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            throw std::runtime_error("malformed JSON unicode codepoint");
        }
    }

    unsigned parse_hex4() {
        if (position_ + 4 > source_.size())
            fail("short unicode escape");
        unsigned value = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const auto character = source_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9')
                value += static_cast<unsigned>(character - '0');
            else if (character >= 'a' && character <= 'f')
                value += static_cast<unsigned>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F')
                value += static_cast<unsigned>(character - 'A' + 10);
            else
                fail("invalid unicode escape");
        }
        return value;
    }

    std::string parse_string() {
        if (!consume('"'))
            fail("expected string");
        std::string result;
        while (position_ < source_.size()) {
            const auto character = source_[position_++];
            if (character == '"')
                return result;
            if (static_cast<unsigned char>(character) < 0x20)
                fail("control character in string");
            if (character != '\\') {
                result.push_back(character);
                continue;
            }
            if (position_ >= source_.size())
                fail("unterminated escape");
            switch (source_[position_++]) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '/':
                result.push_back('/');
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u': {
                auto value = parse_hex4();
                if (value >= 0xd800 && value <= 0xdbff) {
                    if (position_ + 6 > source_.size() || source_[position_] != '\\' ||
                        source_[position_ + 1] != 'u')
                        fail("high surrogate without low surrogate");
                    position_ += 2;
                    const auto low = parse_hex4();
                    if (low < 0xdc00 || low > 0xdfff)
                        fail("invalid low surrogate");
                    value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
                } else if (value >= 0xdc00 && value <= 0xdfff) {
                    fail("unexpected low surrogate");
                }
                append_codepoint(result, value);
                break;
            }
            default:
                fail("unknown string escape");
            }
        }
        fail("unterminated string");
    }

    std::string_view source_;
    std::size_t position_ = 0;
};

inline JsonValue parse_json(std::string_view source) {
    return JsonParser(source).parse();
}

} // namespace tts_front::detail
