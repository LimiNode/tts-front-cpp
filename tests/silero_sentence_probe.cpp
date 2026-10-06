#include "tts_front/backend/silero/silero_stress_backend.hpp"

#include <array>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

void emit_trace(const tts_front::detail::SileroSentenceResult& result) {
    std::array<std::size_t, 4> counts{};
    for (const auto& word : result.words) {
        switch (word.route) {
        case tts_front::detail::SileroWordRoute::Model:
            ++counts[0];
            break;
        case tts_front::detail::SileroWordRoute::Exception:
            ++counts[1];
            break;
        case tts_front::detail::SileroWordRoute::Phrase:
            ++counts[2];
            break;
        case tts_front::detail::SileroWordRoute::Homosolver:
            ++counts[3];
            break;
        }
    }
    std::cerr << "TRACE model=" << counts[0] << ";exception=" << counts[1]
              << ";phrase=" << counts[2] << ";homosolver=" << counts[3] << '\n';
    std::cerr << "STRESSED";
    for (const auto& word : result.words) {
        std::cerr << ' ';
        if (word.stressed_vowel) {
            std::cerr << *word.stressed_vowel;
        } else {
            std::cerr << '-';
        }
    }
    std::cerr << '\n';
}

} // namespace

int main(int argc, char** argv) {
#if !defined(TTS_FRONT_ENABLE_ONNX_STRESS)
    (void)argc;
    (void)argv;
    std::cerr << "silero sentence probe requires ONNX Runtime support\n";
    return 2;
#else
    if (argc != 3 || std::string(argv[1]) != "--bundle") {
        std::cerr << "usage: silero_sentence_probe --bundle DIR\n";
        return 2;
    }
    try {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        tts_front::detail::SileroStressBackend backend({argv[2], "1.30.0", 1U << 20});
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto result = backend.process(line);
            std::cout << result.pronunciation_text << '\n';
            std::cout.flush();
            emit_trace(result);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "silero sentence probe failed: " << error.what() << '\n';
        return 1;
    }
#endif
}
