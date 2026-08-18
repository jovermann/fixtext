// Misc utility functions.
//
// Copyright (c) 2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#include "MiscUtils.hpp"
#include "UnitTest.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace ut1
{

static std::string compilerDateToIso(std::string_view date)
{
    const std::string monthName(date.substr(0, 3));
    std::string month = "01";
    if (monthName == "Feb") { month = "02"; }
    else if (monthName == "Mar") { month = "03"; }
    else if (monthName == "Apr") { month = "04"; }
    else if (monthName == "May") { month = "05"; }
    else if (monthName == "Jun") { month = "06"; }
    else if (monthName == "Jul") { month = "07"; }
    else if (monthName == "Aug") { month = "08"; }
    else if (monthName == "Sep") { month = "09"; }
    else if (monthName == "Oct") { month = "10"; }
    else if (monthName == "Nov") { month = "11"; }
    else if (monthName == "Dec") { month = "12"; }
    std::string day(date.substr(4, 2));
    if (day[0] == ' ')
    {
        day[0] = '0';
    }
    return std::string(date.substr(7, 4)) + "-" + month + "-" + day;
}

std::string getCompileDate()
{
    return compilerDateToIso(__DATE__);
}

UNIT_TEST(getCompileDate)
{
    ASSERT_EQ(compilerDateToIso("Jan  1 2026"), "2026-01-01");
    ASSERT_EQ(compilerDateToIso("Dec 31 2026"), "2026-12-31");
}

bool hasPrefix(const std::string& s, const std::string& prefix) noexcept
{
    return s.compare(0, prefix.length(), prefix) == 0;
}

void replaceStringInPlace(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }
    for (size_t pos = 0;; pos += to.size())
    {
        pos = s.find(from, pos);
        if (pos == std::string::npos)
        {
            break;
        }
        s.replace(pos, from.size(), to);
    }
}

void skipSpace(const char*& s) noexcept
{
    while (s && std::isspace(static_cast<unsigned char>(*s)))
    {
        s++;
    }
}

std::vector<std::string> splitString(const std::string& s, char sep, int maxSplit)
{
    std::vector<std::string> r;
    if (s.empty())
    {
        return r;
    }
    size_t start = 0;
    while ((maxSplit != 0) && (start <= s.size()))
    {
        size_t end = s.find(sep, start);
        if (end == std::string::npos)
        {
            break;
        }
        r.push_back(s.substr(start, end - start));
        start = end + 1;
        if (maxSplit > 0)
        {
            maxSplit--;
        }
    }
    r.push_back(s.substr(start));
    return r;
}

std::vector<std::string> splitLines(const std::string& s, size_t wrapCol)
{
    std::vector<std::string> raw;
    size_t start = 0;
    while (start < s.size())
    {
        size_t end = s.find('\n', start);
        if (end == std::string::npos)
        {
            end = s.size();
        }
        raw.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    if (!s.empty() && (s.back() == '\n') && !raw.empty())
    {
        raw.pop_back();
    }

    if (wrapCol == 0)
    {
        return raw;
    }

    std::vector<std::string> wrapped;
    for (const std::string& line: raw)
    {
        if (line.size() <= wrapCol)
        {
            wrapped.push_back(line);
            continue;
        }
        size_t pos = 0;
        while (pos < line.size())
        {
            size_t len = std::min(wrapCol, line.size() - pos);
            size_t cut = line.rfind(' ', pos + len);
            if ((len == wrapCol) && (cut != std::string::npos) && (cut > pos))
            {
                len = cut - pos;
            }
            wrapped.push_back(line.substr(pos, len));
            pos += len;
            while ((pos < line.size()) && (line[pos] == ' '))
            {
                pos++;
            }
        }
    }
    return wrapped;
}

std::string joinStrings(const std::vector<std::string>& stringList, const std::string& sep)
{
    std::string r;
    for (const std::string& s: stringList)
    {
        if (!r.empty())
        {
            r += sep;
        }
        r += s;
    }
    return r;
}

std::string expandUnprintable(const std::string& s, char quotes, char addQuotes)
{
    std::string r;
    char buf[8]{};
    if (addQuotes)
    {
        r += addQuotes;
    }
    for (char c: s)
    {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isprint(uc))
        {
            if ((c == '\\') || (quotes && (c == quotes)))
            {
                r += '\\';
            }
            r += c;
        }
        else
        {
            switch (c)
            {
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                std::snprintf(buf, sizeof(buf), "\\x%02x", uc);
                r += buf;
                break;
            }
        }
    }
    if (addQuotes)
    {
        r += addQuotes;
    }
    return r;
}

} // namespace ut1
