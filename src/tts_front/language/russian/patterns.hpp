#pragma once

#include <regex>

namespace tts_front::detail::russian {

struct Patterns {
    const std::regex date{R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)"};
    const std::regex year{R"(\b(\d{4})\s*г\.)"};
    const std::regex decimal_percent{R"((-?\d+),([0-9]+)\s*%([^0-9]|$))"};
    const std::regex currency_decimal{
        R"((-?\d+),([0-9]+)\s*(рублей|рубля|рубль|руб\.?)([^0-9A-Za-z]|$))"};
    const std::regex percent{R"((-?\d+)\s*%)"};
    const std::regex currency{R"((-?\d+)\s*(рублей|рубля|рубль|руб\.?)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex time{R"(\b(\d{1,2}):(\d{2})\b)"};
    const std::regex decimal{R"((-?\d+)[,](\d+)([^0-9]|$))"};
    const std::regex measurement{
        R"((-?\d+)\s*(километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex generic_number{R"((^|[^A-Za-z0-9_,.:])(-?\d+)(?![0-9]*[.,:][0-9]))"};
    const std::regex abbreviation_td{R"((^|[^A-Za-zА-Яа-яЁё])т\.д\.)"};
    const std::regex abbreviation_tp{R"((^|[^A-Za-zА-Яа-яЁё])т\.п\.)"};
};

const Patterns& patterns();

} // namespace tts_front::detail::russian
