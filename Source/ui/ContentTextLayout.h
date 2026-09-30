#pragma once
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace er
{
// No JUCE dependency: the caller supplies actual glyph widths. This makes
// wrapping/overflow testable without a window system or a font installation.
struct ContentTextLines
{
    std::vector<std::u32string> lines;
    bool overflow = false;
};

template <typename Measure>
ContentTextLines layoutContentText(const std::u32string& source, float width,
                                  int maxLines, const Measure& measure)
{
    ContentTextLines result;
    if (source.empty()) return result;
    if (width <= 0.0f || maxLines <= 0)
    {
        result.overflow = true;
        return result;
    }

    std::u32string text;
    text.reserve(source.size());
    for (size_t i = 0; i < source.size(); ++i)
    {
        const auto c = source[i];
        if (c == U'\r')
        {
            text += U'\n';
            if (i + 1 < source.size() && source[i + 1] == U'\n') ++i;
        }
        else if (c == U'\t') text += U"    ";
        else text += c;
    }

    // Find the largest prefix that fits, measured with the rendering font.
    const auto fittingPrefix = [&] (const std::u32string& line, const std::u32string& suffix)
    {
        size_t low = 0, high = line.size();
        while (low < high)
        {
            const auto mid = low + (high - low + 1) / 2;
            if (measure(line.substr(0, mid) + suffix) <= width) low = mid;
            else high = mid - 1;
        }
        return low;
    };

    size_t offset = 0;
    while (offset < text.size() && (int) result.lines.size() < maxLines)
    {
        const auto newline = text.find(U'\n', offset);
        const auto end = newline == std::u32string::npos ? text.size() : newline;
        const auto remaining = text.substr(offset, end - offset);
        const auto fit = fittingPrefix(remaining, {});
        if (fit == remaining.size())
        {
            result.lines.push_back(remaining);
            offset = end < text.size() ? end + 1 : end;
        }
        else
        {
            // Prefer a word boundary, but split a long unbroken word too.
            // Always consume at least one code point to guarantee progress.
            auto count = std::max(size_t(1), fit);
            const auto space = remaining.rfind(U' ', fit);
            if (space != std::u32string::npos && space > 0) count = space;
            auto line = remaining.substr(0, count);
            while (!line.empty() && line.back() == U' ') line.pop_back();
            result.lines.push_back(std::move(line));
            offset += count;
            while (offset < end && text[offset] == U' ') ++offset;
            if (offset == end && end < text.size()) ++offset;
        }
    }

    result.overflow = offset < text.size();
    if (result.overflow && !result.lines.empty())
    {
        auto& last = result.lines.back();
        last.resize(fittingPrefix(last, U"..."));
        while (!last.empty() && last.back() == U' ') last.pop_back();
        last += U"...";
    }
    return result;
}
}
