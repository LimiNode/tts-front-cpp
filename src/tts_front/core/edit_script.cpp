#include "edit_script.hpp"

#include <algorithm>

namespace tts_front::detail {

MappedText apply_source_edits(const MappedText& input, std::vector<SourceEdit> edits) {
    std::sort(edits.begin(), edits.end(), [](const SourceEdit& left, const SourceEdit& right) {
        return left.begin < right.begin;
    });
    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    for (const auto& edit : edits) {
        if (edit.begin < cursor || edit.begin > edit.end || edit.end > input.text.size())
            continue;
        output.append_copy(input, cursor, edit.begin);
        output.append_generated(input, edit.begin, edit.end, edit.replacement);
        cursor = edit.end;
    }
    output.append_copy(input, cursor, input.text.size());
    return output;
}

} // namespace tts_front::detail
