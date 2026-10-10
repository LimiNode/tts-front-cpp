#pragma once

#include <regex>
#include <string_view>

namespace tts_front::detail::technical {

struct Patterns {
    const std::regex technical_url{R"(https?://[^\s]+)"};
    const std::regex technical_email{R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})"};
    const std::regex technical_ipv4{R"(\b\d{1,3}(?:\.\d{1,3}){3}\b)"};
    const std::regex technical_version{R"(\b[vV]\d+(?:\.\d+)+\b)"};
    const std::regex technical_http{R"(\bHTTP/\d+(?:\.\d+)?\b)"};
    const std::regex technical_gpu{R"(\b(?:RTX|CUDA|GPU|API)\s+\d+(?:\.\d+)?\b)"};
    const std::regex technical_cpp{R"(C\+\+)"};
    const std::regex technical_csharp{R"(C#)"};
    const std::regex technical_identifier{
        R"((?:#[0-9]+)|(?:[A-Za-z][A-Za-z0-9+._$#-]*[-+$][A-Za-z0-9._$#-]+))"};
    const std::regex technical_numeric_percent{
        R"(((?:[vV]\d+(?:\.\d+)+|HTTP/\d+(?:\.\d+)?|C#\d+(?:\.\d+)?|(?:RTX|CUDA|GPU|API)\s+\d+(?:\.\d+)?|\d{1,3}(?:\.\d{1,3}){3}|#[0-9]+|[A-Za-z][A-Za-z0-9+._#-]*\$(?:[+-][ \t]*)?\d+|[A-Za-z][A-Za-z0-9+._$#-]*[-+$][ \t]*[A-Za-z0-9._$#-]+)))"};
};

const Patterns& patterns();

bool has_known_identifier_prefix(std::string_view value);

} // namespace tts_front::detail::technical
