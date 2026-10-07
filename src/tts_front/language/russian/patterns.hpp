#pragma once

#include <regex>

namespace tts_front::detail::russian {

struct Patterns {
    const std::regex ru_date{R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)"};
    const std::regex ru_year{R"(\b(\d{4})\s*г\.)"};
    const std::regex ru_decimal_percent{R"((-?\d+),([0-9]+)\s*%([^0-9]|$))"};
    const std::regex ru_percent{R"((-?\d+)\s*%)"};
    const std::regex ru_currency{
        R"((-?\d+)\s*(рублей|рубля|рубль|руб\.?)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex ru_time{R"(\b(\d{1,2}):(\d{2})\b)"};
    const std::regex ru_decimal{R"((-?\d+)[,](\d+)([^0-9]|$))"};
    const std::regex ru_measurement{
        R"((-?\d+)\s*(километров|километра|километр|км|килограммов|килограмма|килограмм|кг|сантиметров|сантиметра|сантиметр|см|миллиметров|миллиметра|миллиметр|мм|ГБ|МБ|м)([^А-Яа-яЁёA-Za-z0-9]|$))"};
    const std::regex generic_ru_number{R"((^|[^A-Za-z0-9_,.:])(-?\d+)(?![0-9]*[.,:][0-9]))"};
    const std::regex ru_abbreviation_td{R"((^|[^A-Za-zА-Яа-яЁё])т\.д\.)"};
    const std::regex ru_abbreviation_tp{R"((^|[^A-Za-zА-Яа-яЁё])т\.п\.)"};
};

} // namespace tts_front::detail::russian
