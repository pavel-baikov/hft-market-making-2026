#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cmf
{

// 2 index columns + 25 levels * 4 (ask_price, ask_qty, bid_price, bid_qty).
static constexpr std::size_t MAX_CSV_FIELDS = 102;

inline std::size_t splitCsvLine(std::string_view line,
                                std::array<std::string_view, MAX_CSV_FIELDS>& out)
{
    std::size_t count = 0;
    std::size_t start = 0;
    while (count < out.size())
    {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos)
        {
            if (start <= line.size())
            {
                out[count++] = line.substr(start);
            }
            break;
        }
        out[count++] = line.substr(start, comma - start);
        start = comma + 1;
    }
    return count;
}

inline double parseDouble(std::string_view value)
{
    double result = 0.0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc() || parsed.ptr != last)
    {
        throw std::runtime_error("invalid floating point value: " + std::string(value));
    }
    return result;
}

inline std::int64_t parseI64(std::string_view value)
{
    std::int64_t result = 0;
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc() || parsed.ptr != last)
    {
        throw std::runtime_error("invalid integer value: " + std::string(value));
    }
    return result;
}

} // namespace cmf
