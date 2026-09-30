#include <iostream>
#include "tts_front/tts_front.hpp"
int main() {
  tts_front::TextFrontend frontend; tts_front::TextFrontendOptions options; options.language = tts_front::Language::English;
  std::cout << frontend.process("GPU: RTX 4090, 7.5% at 12:35", options).normalized_text << "\n"; return 0;
}
