#include <iostream>
#include "tts_front/tts_front.hpp"
int main() {
  tts_front::TextFrontend frontend; tts_front::TextFrontendOptions options; options.language = tts_front::Language::Russian;
  const auto result = frontend.process("В 2026 г. RTX 4090 стоит 12 500 руб.", options);
  std::cout << result.normalized_text << "\n"; return 0;
}
