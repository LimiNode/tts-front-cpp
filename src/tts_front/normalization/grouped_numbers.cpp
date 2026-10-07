#include "tts_front/core/utf8.hpp"
#include "tts_front/normalization/admission.hpp"
#include "tts_front/normalization/codepoint_classification.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <vector>

namespace tts_front::detail {

using CodePoint = Utf8CodePoint;

namespace {

const std::regex& grouped_number_pattern() {
    static const std::regex pattern{R"((^|[^0-9])-?\d{1,3}(?:\s+\d{3})+(?![0-9]))"};
    return pattern;
}

} // namespace

MappedText collapse_grouped_numbers(const MappedText& input) {
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(input.text.begin(), input.text.end(), grouped_number_pattern()),
         end;
         it != end;
         ++it) {
        const auto begin = static_cast<std::size_t>(it->position());
        const auto finish = begin + static_cast<std::size_t>(it->length());
        const auto number_begin = begin + it->length(1);
        output.append_copy(input, cursor, number_begin);
        std::string number = it->str().substr(it->length(1));
        number.erase(std::remove_if(number.begin(),
                                    number.end(),
                                    [](unsigned char c) { return std::isspace(c) != 0; }),
                     number.end());
        output.append_generated(input, number_begin, finish, number);
        cursor = finish;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

} // namespace tts_front::detail
