#include <chrono>
#include <iostream>
#include "tts_front/tts_front.hpp"
int main() {
  const std::string text = "В 2026 году GPU RTX 4090 обработал 12500 запросов.";
  tts_front::TextFrontend frontend; tts_front::TextFrontendOptions options; options.language = tts_front::Language::Russian;
  constexpr int iterations = 10000; const auto begin = std::chrono::steady_clock::now();
  for (int i=0;i<iterations;++i) (void)frontend.process(text, options);
  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
  std::cout << iterations << " iterations, " << elapsed << " s, " << (elapsed/iterations*1e6) << " us/request\n";
}
