#pragma once

#include "mapped_text.hpp"

#include <vector>

namespace tts_front::detail {

MappedText apply_source_edits(const MappedText& input, std::vector<SourceEdit> edits);

} // namespace tts_front::detail
