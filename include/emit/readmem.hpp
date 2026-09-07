#pragma once

#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wolvrix::lib::emit
{
    inline bool tokenizeReadmemText(std::string_view text, std::vector<std::string> &tokens, std::string &error)
    {
        tokens.clear();
        for (std::size_t i = 0; i < text.size();)
        {
            if (std::isspace(static_cast<unsigned char>(text[i]))) { ++i; continue; }
            if (text[i] == '/' && i + 1 < text.size())
            {
                if (text[i + 1] == '/')
                {
                    i += 2;
                    while (i < text.size() && text[i] != '\n') ++i;
                    continue;
                }
                if (text[i + 1] == '*')
                {
                    const auto end = text.find("*/", i + 2);
                    if (end == std::string_view::npos)
                    {
                        error = "unterminated block comment in readmem file";
                        return false;
                    }
                    i = end + 2;
                    continue;
                }
            }
            const auto begin = i;
            while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
            {
                if (text[i] == '/' && i + 1 < text.size() && (text[i + 1] == '/' || text[i + 1] == '*')) break;
                ++i;
            }
            if (i > begin) tokens.emplace_back(text.substr(begin, i - begin));
        }
        return true;
    }

    inline std::optional<std::size_t> parseReadmemAddress(std::string_view token)
    {
        std::size_t value = 0;
        bool hasDigit = false;
        for (unsigned char ch : token)
        {
            if (ch == '_') continue;
            unsigned digit;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
            else return std::nullopt;
            if (value > (std::numeric_limits<std::size_t>::max() - digit) / 16) return std::nullopt;
            value = value * 16 + digit;
            hasDigit = true;
        }
        return hasDigit ? std::optional<std::size_t>(value) : std::nullopt;
    }
}
