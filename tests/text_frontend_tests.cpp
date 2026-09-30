#include <cassert>
#include <iostream>
#include "tts_front/tts_front.hpp"
using namespace tts_front;
int main() {
  TextFrontend frontend; TextFrontendOptions ru; ru.language = Language::Russian;
  auto r = frontend.process("У меня 42 яблока.", ru); assert(r.normalized_text.find("сорок два") != std::string::npos); assert(!r.has_uncertainty());
  TextFrontendOptions en; en.language = Language::English;
  auto e = frontend.process("There are 12 GPUs.", en); assert(e.normalized_text.find("twelve") != std::string::npos);
  PronunciationDictionary dict; dict.add_token("API", "эйпиай", 0); dict.add_case_insensitive_token("Qwen", "квен"); TextFrontendOptions d; d.language=Language::English; d.dictionary=&dict;
  auto p = frontend.process("API QWEN", d); assert(p.pronunciation_text == "эйпиай квен"); assert(p.words.front().from_dictionary); assert(p.words.front().stressed_vowel == 0);
  TextFrontendOptions a; a.language=Language::Russian; a.stress_mode=StressMode::Automatic; auto ar=frontend.process("тест", a); assert(!ar.warnings.empty());
  std::cout << "tts_front tests passed\n";
}
