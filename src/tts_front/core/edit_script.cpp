#include "edit_script.hpp"

#include <algorithm>
#include <stdexcept>

namespace tts_front::detail {

MappedText apply_source_edits(const MappedText& input, std::vector<SourceEdit> edits) {
    std::sort(edits.begin(), edits.end(), [](const SourceEdit& left, const SourceEdit& right) {
        return left.begin < right.begin;
    });
    std::size_t previous_end = 0;
    for (const auto& edit : edits) {
        if (edit.begin > edit.end || edit.end > input.text.size() || edit.begin < previous_end)
            throw std::logic_error("overlapping or out-of-range source edit");
        previous_end = edit.end;
    }

    MappedText output;
    output.preserved_ranges = input.preserved_ranges;
    std::size_t cursor = 0;
    std::size_t run_cursor = 0;
    for (const auto& edit : edits) {
        output.append_copy_with_cursor(input, cursor, edit.begin, run_cursor);
        output.append_generated(input, edit.begin, edit.end, edit.replacement);
        cursor = edit.end;
    }
    output.append_copy_with_cursor(input, cursor, input.text.size(), run_cursor);
    return output;
}

} // namespace tts_front::detail
