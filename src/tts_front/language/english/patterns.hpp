#pragma once

#include <regex>

namespace tts_front::detail::english {

struct Patterns {
    const std::regex comma_grouped_number{R"((^|[^0-9])(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?))"};
    const std::regex comma_grouped_value{R"(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?)"};
    const std::regex comma_grouped_percent{R"(-?\d{1,3}(?:,\d{3})+(?:\.\d+)?%)"};
    const std::regex currency_decimal{R"(\$([0-9]+)\.([0-9]{1,}))"};
    const std::regex currency_integer{R"(\$([0-9]+)(?![0-9]|\.[0-9]))"};
    const std::regex ordinal{R"((^|[^A-Za-z0-9-])(-?[0-9]+)(st|nd|rd|th)\b)"};
    const std::regex percent{R"((-?[0-9]+(?:\.[0-9]+)?)\s*%)"};
    const std::regex time{R"(\b(\d{1,2}):(\d{2})\b)"};
    const std::regex decimal{R"((^|[^$A-Za-z0-9])(-?\d+\.\d+))"};
    const std::regex measurement{
        R"((-?\d+)\s*(kilometers|kilometres|kilometer|kilometre|km|kilogram|kilograms|kg|meters|metres|meter|m|centimeters|centimetres|centimeter|cm|millimeters|millimetres|millimeter|mm|GB|MB)([^A-Za-z0-9]|$))"};
    const std::regex generic_number{R"((^|[^A-Za-z0-9_,.:])(-?\d+)(?![0-9]*[.,:][0-9]))"};
};

const Patterns& patterns();

} // namespace tts_front::detail::english
