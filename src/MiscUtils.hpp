// Misc utility functions.
//
// Copyright (c) 2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#pragma once

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ut1
{

bool hasPrefix(const std::string& s, const std::string& prefix) noexcept;
void replaceStringInPlace(std::string& s, const std::string& from, const std::string& to);
void skipSpace(const char*& s) noexcept;
std::string getCompileDate();
std::vector<std::string> splitString(const std::string& s, char sep, int maxSplit = -1);
std::vector<std::string> splitLines(const std::string& s, size_t wrapCol = 0);
std::string joinStrings(const std::vector<std::string>& stringList, const std::string& sep);
std::string expandUnprintable(const std::string& s, char quotes = 0, char addQuotes = 0);

template<typename T>
std::string toStr(const T& t)
{
    std::stringstream r;
    r << t;
    return r.str();
}

inline std::string toStr(const std::string& s)
{
    return expandUnprintable(s, '"', '"');
}

inline std::string hexByte(uint8_t byte)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return os.str();
}

} // namespace ut1
