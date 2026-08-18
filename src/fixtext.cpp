// fixtext - Analyze and fix plain text encodings.
//
// Copyright (c) 2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#include "CommandLineParser.hpp"
#include "MiscUtils.hpp"
#include "UnitTest.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Utf8Sequence
{
    size_t pos{};
    size_t len{};
    uint32_t codePoint{};
};

struct FileAnalysis
{
    std::array<uint64_t, 256> hist{};
    std::array<uint64_t, 256> singleByteCharHist{};
    std::vector<Utf8Sequence> utf8Sequences;
    std::map<uint32_t, uint64_t> utf8CharHist;
    uint64_t bytes{};
    uint64_t asciiBytes{};
    uint64_t nulBytes{};
    uint64_t suspiciousControlBytes{};
    uint64_t latin1GermanBytes{};
};

static constexpr std::array<uint8_t, 7> kGermanLatin1Bytes{0xc4, 0xd6, 0xdc, 0xdf, 0xe4, 0xf6, 0xfc};

static bool isGermanLatin1Byte(uint8_t b)
{
    for (uint8_t x: kGermanLatin1Bytes)
    {
        if (b == x)
        {
            return true;
        }
    }
    return false;
}

static bool isTextWhitespaceControl(uint8_t b)
{
    return (b == '\t') || (b == '\n') || (b == '\r');
}

static bool isAsciiAlnum(uint8_t b)
{
    return ((b >= '0') && (b <= '9')) || ((b >= 'A') && (b <= 'Z')) || ((b >= 'a') && (b <= 'z'));
}

static bool isNotAsciiAlnum(uint8_t b)
{
    return !isAsciiAlnum(b);
}

static std::string utf8Encode(uint32_t cp)
{
    std::string out;
    if (cp <= 0x7f)
    {
        out += static_cast<char>(cp);
    }
    else if (cp <= 0x7ff)
    {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
    else if (cp <= 0xffff)
    {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
    else
    {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
    return out;
}

static std::optional<uint32_t> latin1ToUnicode(uint8_t b)
{
    return static_cast<uint32_t>(b);
}

static std::string asciiControlName(uint8_t b);

static std::string latin1C1ControlName(uint8_t b)
{
    switch (b)
    {
    case 0x80: return "PAD";
    case 0x81: return "HOP";
    case 0x82: return "BPH";
    case 0x83: return "NBH";
    case 0x84: return "IND";
    case 0x85: return "NEL";
    case 0x86: return "SSA";
    case 0x87: return "ESA";
    case 0x88: return "HTS";
    case 0x89: return "HTJ";
    case 0x8a: return "VTS";
    case 0x8b: return "PLD";
    case 0x8c: return "PLU";
    case 0x8d: return "RI";
    case 0x8e: return "SS2";
    case 0x8f: return "SS3";
    case 0x90: return "DCS";
    case 0x91: return "PU1";
    case 0x92: return "PU2";
    case 0x93: return "STS";
    case 0x94: return "CCH";
    case 0x95: return "MW";
    case 0x96: return "SPA";
    case 0x97: return "EPA";
    case 0x98: return "SOS";
    case 0x99: return "SGCI";
    case 0x9a: return "SCI";
    case 0x9b: return "CSI";
    case 0x9c: return "ST";
    case 0x9d: return "OSC";
    case 0x9e: return "PM";
    case 0x9f: return "APC";
    default: break;
    }
    return "";
}

static std::string latin1CharName(uint8_t b)
{
    switch (b)
    {
    case 0xc4: return "Ä";
    case 0xd6: return "Ö";
    case 0xdc: return "Ü";
    case 0xdf: return "ß";
    case 0xe4: return "ä";
    case 0xf6: return "ö";
    case 0xfc: return "ü";
    default: break;
    }
    if ((b >= 0x20) && (b <= 0x7e))
    {
        if (b == 0x20)
        {
            return "SP";
        }
        return std::string(1, static_cast<char>(b));
    }
    if ((b < 0x20) || (b == 0x7f))
    {
        return asciiControlName(b);
    }
    if ((b >= 0x80) && (b <= 0x9f))
    {
        return latin1C1ControlName(b);
    }
    if (b == 0xa0)
    {
        return "NBSP";
    }
    if (b >= 0xa0)
    {
        return utf8Encode(b);
    }
    return "";
}

static std::string asciiControlName(uint8_t b)
{
    switch (b)
    {
    case 0x00: return "NUL";
    case 0x01: return "SOH";
    case 0x02: return "STX";
    case 0x03: return "ETX";
    case 0x04: return "EOT";
    case 0x05: return "ENQ";
    case 0x06: return "ACK";
    case 0x07: return "BEL";
    case 0x08: return "BS";
    case 0x09: return "TAB";
    case 0x0a: return "LF";
    case 0x0b: return "VT";
    case 0x0c: return "FF";
    case 0x0d: return "CR";
    case 0x0e: return "SO";
    case 0x0f: return "SI";
    case 0x10: return "DLE";
    case 0x11: return "DC1";
    case 0x12: return "DC2";
    case 0x13: return "DC3";
    case 0x14: return "DC4";
    case 0x15: return "NAK";
    case 0x16: return "SYN";
    case 0x17: return "ETB";
    case 0x18: return "CAN";
    case 0x19: return "EM";
    case 0x1a: return "SUB";
    case 0x1b: return "ESC";
    case 0x1c: return "FS";
    case 0x1d: return "GS";
    case 0x1e: return "RS";
    case 0x1f: return "US";
    case 0x7f: return "DEL";
    default: break;
    }
    return "";
}

static size_t displayWidth(std::string_view s)
{
    size_t width = 0;
    for (size_t i = 0; i < s.size();)
    {
        const auto b = static_cast<uint8_t>(s[i]);
        if (b < 0x80)
        {
            i++;
        }
        else if ((b & 0xe0) == 0xc0)
        {
            i += 2;
        }
        else if ((b & 0xf0) == 0xe0)
        {
            i += 3;
        }
        else if ((b & 0xf8) == 0xf0)
        {
            i += 4;
        }
        else
        {
            i++;
        }
        width++;
    }
    return width;
}

static std::string cellLeft(const std::string& s, size_t width)
{
    const size_t w = displayWidth(s);
    return s + std::string((w < width) ? (width - w) : 0, ' ');
}

static std::string codePointName(uint32_t cp)
{
    switch (cp)
    {
    case 0x00c4: return "Ä LATIN CAPITAL LETTER A WITH DIAERESIS";
    case 0x00d6: return "Ö LATIN CAPITAL LETTER O WITH DIAERESIS";
    case 0x00dc: return "Ü LATIN CAPITAL LETTER U WITH DIAERESIS";
    case 0x00df: return "ß LATIN SMALL LETTER SHARP S";
    case 0x00e4: return "ä LATIN SMALL LETTER A WITH DIAERESIS";
    case 0x00f6: return "ö LATIN SMALL LETTER O WITH DIAERESIS";
    case 0x00fc: return "ü LATIN SMALL LETTER U WITH DIAERESIS";
    case 0xfffd: return "� REPLACEMENT CHARACTER";
    default: break;
    }
    if ((cp >= 0x20) && (cp <= 0x7e))
    {
        return std::string(1, static_cast<char>(cp));
    }
    std::ostringstream os;
    os << "U+" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << cp;
    return os.str();
}

static std::string codePointId(uint32_t cp)
{
    std::ostringstream os;
    os << "U+" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << cp;
    return os.str();
}

static std::string codePointChar(uint32_t cp)
{
    if (cp < 0x80)
    {
        if (cp == 0x20)
        {
            return "SP";
        }
        if ((cp >= 0x20) && (cp <= 0x7e))
        {
            return std::string(1, static_cast<char>(cp));
        }
        return asciiControlName(static_cast<uint8_t>(cp));
    }
    if ((cp >= 0xd800) && (cp <= 0xdfff))
    {
        return "";
    }
    return utf8Encode(cp);
}

static bool decodeUtf8At(const std::vector<uint8_t>& bytes, size_t pos, Utf8Sequence& seq)
{
    const uint8_t b0 = bytes[pos];
    size_t len = 0;
    uint32_t cp = 0;
    if ((b0 >= 0xc2) && (b0 <= 0xdf))
    {
        len = 2;
        cp = b0 & 0x1f;
    }
    else if ((b0 >= 0xe0) && (b0 <= 0xef))
    {
        len = 3;
        cp = b0 & 0x0f;
    }
    else if ((b0 >= 0xf0) && (b0 <= 0xf4))
    {
        len = 4;
        cp = b0 & 0x07;
    }
    else
    {
        return false;
    }
    if (pos + len > bytes.size())
    {
        return false;
    }
    for (size_t i = 1; i < len; i++)
    {
        if ((bytes[pos + i] & 0xc0) != 0x80)
        {
            return false;
        }
        cp = (cp << 6) | (bytes[pos + i] & 0x3f);
    }
    if (((len == 2) && (cp < 0x80)) || ((len == 3) && (cp < 0x800)) || ((len == 4) && (cp < 0x10000)))
    {
        return false;
    }
    if ((cp >= 0xd800) && (cp <= 0xdfff))
    {
        return false;
    }
    seq = Utf8Sequence{pos, len, cp};
    return true;
}

static FileAnalysis analyzeBytes(const std::vector<uint8_t>& bytes)
{
    FileAnalysis a;
    a.bytes = bytes.size();
    for (uint8_t b: bytes)
    {
        a.hist[b]++;
        if (b < 0x80)
        {
            a.asciiBytes++;
        }
        if (b == 0)
        {
            a.nulBytes++;
        }
        if ((b < 0x20) && !isTextWhitespaceControl(b))
        {
            a.suspiciousControlBytes++;
        }
        if (isGermanLatin1Byte(b))
        {
            a.latin1GermanBytes++;
        }
    }

    for (size_t i = 0; i < bytes.size();)
    {
        Utf8Sequence seq;
        if (decodeUtf8At(bytes, i, seq))
        {
            a.utf8Sequences.push_back(seq);
            a.utf8CharHist[seq.codePoint]++;
            i += seq.len;
        }
        else
        {
            a.singleByteCharHist[bytes[i]]++;
            i++;
        }
    }
    return a;
}

static bool looksBinary(const FileAnalysis& a)
{
    if (a.bytes == 0)
    {
        return false;
    }
    if (a.nulBytes > 0)
    {
        return true;
    }
    const double suspiciousPct = 100.0 * static_cast<double>(a.suspiciousControlBytes) / static_cast<double>(a.bytes);
    return suspiciousPct > 5.0;
}

static std::string fileContentClasses(const FileAnalysis& a)
{
    std::vector<std::string> classes;
    bool hasAscii = false;
    bool hasLatin1 = false;
    bool hasUtf8 = !a.utf8CharHist.empty();
    if (looksBinary(a))
    {
        return "binary";
    }
    for (size_t i = 0; i < 0x80; i++)
    {
        if (a.singleByteCharHist[i])
        {
            hasAscii = true;
            break;
        }
    }
    for (size_t i = 0x80; i < a.singleByteCharHist.size(); i++)
    {
        if (a.singleByteCharHist[i])
        {
            hasLatin1 = true;
            break;
        }
    }
    if (hasLatin1)
    {
        classes.emplace_back("latin1");
    }
    if (hasUtf8)
    {
        classes.emplace_back("utf-8");
    }
    if (hasAscii && !hasLatin1 && !hasUtf8)
    {
        classes.emplace_back("ascii");
    }
    if (a.utf8CharHist.count(0xfffd))
    {
        classes.emplace_back("utf-replacement");
    }
    if (classes.empty())
    {
        classes.emplace_back("empty");
    }
    return ut1::joinStrings(classes, "+");
}

static std::vector<uint8_t> readFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void writeFile(const fs::path& path, const std::string& data)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("Cannot write " + path.string());
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out)
    {
        throw std::runtime_error("Write failed for " + path.string());
    }
}

static std::string fixLatin1GermanUmlauts(const std::vector<uint8_t>& bytes, size_t* convertedOut = nullptr)
{
    std::string out;
    out.reserve(bytes.size());
    size_t converted = 0;
    for (size_t i = 0; i < bytes.size();)
    {
        Utf8Sequence seq;
        if (decodeUtf8At(bytes, i, seq))
        {
            out.append(reinterpret_cast<const char*>(&bytes[i]), seq.len);
            i += seq.len;
            continue;
        }
        const uint8_t b = bytes[i];
        if (isGermanLatin1Byte(b))
        {
            out += utf8Encode(*latin1ToUnicode(b));
            converted++;
        }
        else
        {
            out += static_cast<char>(b);
        }
        i++;
    }
    if (convertedOut)
    {
        *convertedOut = converted;
    }
    return out;
}

static std::string fixFuerReplacement(const std::string& text, size_t* convertedOut = nullptr)
{
    static const std::string replacement = utf8Encode(0xfffd);
    std::string out;
    out.reserve(text.size());
    size_t converted = 0;
    for (size_t i = 0; i < text.size();)
    {
        const auto b = static_cast<uint8_t>(text[i]);
        if ((i + 3 + replacement.size() < text.size()) &&
            isNotAsciiAlnum(b) &&
            ((text[i + 1] == 'f') || (text[i + 1] == 'F')) &&
            (text.compare(i + 2, replacement.size(), replacement) == 0) &&
            ((text[i + 2 + replacement.size()] == 'r') || (text[i + 2 + replacement.size()] == 'R')) &&
            isNotAsciiAlnum(static_cast<uint8_t>(text[i + 3 + replacement.size()])))
        {
            out += text[i];
            if ((text[i + 1] == 'F') && (text[i + 2 + replacement.size()] == 'R'))
            {
                out += "FÜR";
            }
            else if (text[i + 1] == 'F')
            {
                out += "Für";
            }
            else
            {
                out += "für";
            }
            out += text[i + 3 + replacement.size()];
            i += 4 + replacement.size();
            converted++;
        }
        else
        {
            out += text[i];
            i++;
        }
    }
    if (convertedOut)
    {
        *convertedOut = converted;
    }
    return out;
}

UNIT_TEST(utf8Decode)
{
    std::vector<uint8_t> bytes{'a', 0xc3, 0xa4, 0xe2, 0x82, 0xac};
    FileAnalysis a = analyzeBytes(bytes);
    ASSERT_EQ(a.utf8Sequences.size(), size_t(2));
    ASSERT_EQ(a.utf8Sequences[0].codePoint, uint32_t(0x00e4));
    ASSERT_EQ(a.utf8Sequences[1].codePoint, uint32_t(0x20ac));
}

UNIT_TEST(fixLatin1GermanUmlauts)
{
    const std::vector<uint8_t> bytes{'M', 0xe4, 'x', 0xc3, 0xb6, 0xdf};
    size_t converted = 0;
    const std::string fixed = fixLatin1GermanUmlauts(bytes, &converted);
    ASSERT_EQ(converted, size_t(2));
    ASSERT_EQ(fixed, std::string("Mäxöß"));
}

UNIT_TEST(fixFuerReplacement)
{
    const std::string replacement = utf8Encode(0xfffd);
    size_t converted = 0;
    ASSERT_EQ(fixFuerReplacement(" f" + replacement + "r ", &converted), " für ");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement("\tF" + replacement + "r\n", &converted), "\tFür\n");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement(" F" + replacement + "R ", &converted), " FÜR ");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement("x f" + replacement + "r y", &converted), "x für y");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement("no:f" + replacement + "r\n", &converted), "no:für\n");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement("(f" + replacement + "r)", &converted), "(für)");
    ASSERT_EQ(converted, size_t(1));
    ASSERT_EQ(fixFuerReplacement("f" + replacement + "r", &converted), "f" + replacement + "r");
    ASSERT_EQ(converted, size_t(0));
    ASSERT_EQ(fixFuerReplacement("xf" + replacement + "r ", &converted), "xf" + replacement + "r ");
    ASSERT_EQ(converted, size_t(0));
    ASSERT_EQ(fixFuerReplacement(" f" + replacement + "ry", &converted), " f" + replacement + "ry");
    ASSERT_EQ(converted, size_t(0));
}

UNIT_TEST(binaryHeuristic)
{
    ASSERT_EQ(looksBinary(analyzeBytes(std::vector<uint8_t>{0})), true);
    ASSERT_EQ(looksBinary(analyzeBytes(std::vector<uint8_t>{'a', '\t', '\n', '\r'})), false);
    ASSERT_EQ(looksBinary(analyzeBytes(std::vector<uint8_t>{'a', 'b', 'c', 'd', 0x1b})), true);
    ASSERT_EQ(looksBinary(analyzeBytes(std::vector<uint8_t>{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 0x1b})), true);
    ASSERT_EQ(looksBinary(analyzeBytes(std::vector<uint8_t>{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                                                            'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 0x1b})), false);
}

static std::string bytesForUtf8CodePoint(uint32_t cp)
{
    const std::string bytes = utf8Encode(cp);
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); i++)
    {
        if (i)
        {
            os << ' ';
        }
        os << std::setw(2) << unsigned(static_cast<uint8_t>(bytes[i]));
    }
    return os.str();
}

static std::string singleByteMeaning(uint8_t b)
{
    if ((b < 0x20) || (b == 0x7f))
    {
        return "ASCII control";
    }
    if (b < 0x80)
    {
        return "ASCII";
    }
    if (b < 0xa0)
    {
        return "Latin1 C1 control";
    }
    return "Latin1";
}

static void printCharHistogramRow(const std::string& source, const std::string& value, uint64_t count, const std::string& bytes, const std::string& ch, const std::string& meaning)
{
    std::cout << "    " << cellLeft(source, 10) << cellLeft(value, 14) << std::right << std::setw(8) << count << "  "
              << cellLeft(bytes, 14) << cellLeft(ch, 10) << meaning << "\n";
}

static void printCharHistogram(const FileAnalysis& a, unsigned verbosity)
{
    std::cout << "  char histogram:\n";
    std::cout << "    " << cellLeft("source", 10) << cellLeft("value", 14) << std::right << std::setw(8) << "count" << "  "
              << cellLeft("bytes", 14) << cellLeft("char", 10) << "meaning\n";

    uint64_t printableAscii = 0;
    for (size_t i = 0x20; i <= 0x7e; i++)
    {
        printableAscii += a.singleByteCharHist[i];
    }
    if ((verbosity == 0) && printableAscii)
    {
        printCharHistogramRow("ASCII", "printable", printableAscii, "0x20..0x7e", "", "printable ASCII");
    }

    for (size_t i = 0; i < a.singleByteCharHist.size(); i++)
    {
        const auto count = a.singleByteCharHist[i];
        if ((count == 0) && (verbosity < 2))
        {
            continue;
        }
        const auto b = static_cast<uint8_t>(i);
        if ((verbosity == 0) && (b >= 0x20) && (b <= 0x7e))
        {
            continue;
        }
        printCharHistogramRow(b < 0x80 ? "ASCII" : "Latin1", ut1::hexByte(b), count, ut1::hexByte(b), latin1CharName(b), singleByteMeaning(b));
    }

    for (const auto& kv: a.utf8CharHist)
    {
        printCharHistogramRow("UTF-8", codePointId(kv.first), kv.second, bytesForUtf8CodePoint(kv.first), codePointChar(kv.first), codePointName(kv.first));
    }
}

static std::string byteUtf8Meaning(uint8_t b)
{
    if (b < 0x80)
    {
        return "ASCII";
    }
    if ((b >= 0x80) && (b <= 0xbf))
    {
        return "UTF-8 continuation";
    }
    if ((b >= 0xc2) && (b <= 0xdf))
    {
        return "UTF-8 2-byte lead";
    }
    if ((b >= 0xe0) && (b <= 0xef))
    {
        return "UTF-8 3-byte lead";
    }
    if ((b >= 0xf0) && (b <= 0xf4))
    {
        return "UTF-8 4-byte lead";
    }
    return "invalid UTF-8 lead";
}

static void printRawByteHistogram(const FileAnalysis& a, bool showZeros)
{
    std::cout << "  raw byte histogram (-vv):\n";
    std::cout << "    " << cellLeft("byte", 8) << std::right << std::setw(8) << "dec" << std::setw(10) << "count"
              << "  " << cellLeft("Latin1", 14) << "UTF-8 byte semantics\n";
    for (size_t i = 0; i < a.hist.size(); i++)
    {
        if ((a.hist[i] == 0) && !showZeros)
        {
            continue;
        }
        const uint8_t b = static_cast<uint8_t>(i);
        std::cout << "    " << cellLeft(ut1::hexByte(b), 8) << std::right << std::setw(8) << std::dec << i
                  << std::setw(10) << a.hist[i] << "  " << cellLeft(latin1CharName(b), 14) << byteUtf8Meaning(b) << "\n";
    }
}

static void analyzeFile(const fs::path& path, unsigned analysisLevel)
{
    const std::vector<uint8_t> bytes = readFile(path);
    const FileAnalysis a = analyzeBytes(bytes);
    const bool binary = looksBinary(a);
    if (analysisLevel == 0)
    {
        std::cout << path.string() << ": " << fileContentClasses(a) << "\n";
        return;
    }
    std::cout << path.string() << "\n";
    std::cout << "  bytes: " << a.bytes << ", ASCII: " << a.asciiBytes << ", NUL: " << a.nulBytes
              << ", suspicious controls: " << a.suspiciousControlBytes << ", binary heuristic: " << (binary ? "yes" : "no") << "\n";
    if (binary)
    {
        std::cout << "  skipped detailed text analysis for likely binary file\n";
        if (analysisLevel >= 2)
        {
            printRawByteHistogram(a, analysisLevel >= 3);
        }
        return;
    }
    printCharHistogram(a, analysisLevel - 1);
}

static bool fixFile(const fs::path& path, bool fixLatin1, bool fixFuer, bool dryRun)
{
    const std::vector<uint8_t> bytes = readFile(path);
    const FileAnalysis a = analyzeBytes(bytes);
    if (looksBinary(a))
    {
        return false;
    }
    size_t latin1Converted = 0;
    size_t fuerConverted = 0;
    std::string fixed(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (fixLatin1)
    {
        fixed = fixLatin1GermanUmlauts(std::vector<uint8_t>(fixed.begin(), fixed.end()), &latin1Converted);
    }
    if (fixFuer)
    {
        fixed = fixFuerReplacement(fixed, &fuerConverted);
    }
    const size_t converted = latin1Converted + fuerConverted;
    if (converted == 0)
    {
        return false;
    }
    if (!dryRun)
    {
        writeFile(path, fixed);
    }
    std::cout << path.string() << ":";
    if (dryRun)
    {
        std::cout << " would";
    }
    if (latin1Converted)
    {
        std::cout << " convert " << latin1Converted << " German Latin1 byte" << (latin1Converted == 1 ? "" : "s") << " to UTF-8";
    }
    if (fuerConverted)
    {
        std::cout << (latin1Converted ? "," : "") << (dryRun ? " replace " : " replaced ") << fuerConverted << " damaged fuer sequence" << (fuerConverted == 1 ? "" : "s");
    }
    std::cout << "\n";
    return true;
}

static bool shouldSkipDir(const fs::path& path)
{
    return path.filename() == ".git";
}

static std::vector<fs::path> collectFiles(const std::vector<std::string>& args, bool recursive)
{
    std::vector<fs::path> files;
    for (const std::string& arg: args)
    {
        fs::path path(arg);
        if (fs::is_regular_file(path))
        {
            files.push_back(path);
        }
        else if (fs::is_directory(path))
        {
            if (shouldSkipDir(path))
            {
                continue;
            }
            if (recursive)
            {
                fs::recursive_directory_iterator it(path);
                const fs::recursive_directory_iterator end;
                for (; it != end; ++it)
                {
                    if (it->is_directory() && shouldSkipDir(it->path()))
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    if (it->is_regular_file())
                    {
                        files.push_back(it->path());
                    }
                }
            }
            else
            {
                for (const auto& entry: fs::directory_iterator(path))
                {
                    if (entry.is_regular_file())
                    {
                        files.push_back(entry.path());
                    }
                }
            }
        }
        else
        {
            throw std::runtime_error("Not a file or directory: " + arg);
        }
    }
    return files;
}

static int realMain(int argc, char* argv[])
{
    ut1::CommandLineParser cl(
        "fixtext",
        "Usage: $programName [options] FILE_OR_DIR...\n"
        "Analyze plain text byte encodings and optionally convert German Latin1 umlaut bytes to UTF-8.",
        "\nExamples:\n"
        "  $programName -a notes.txt\n"
        "  $programName -r -a src\n"
        "  $programName -r --fix-latin1 .\n",
        "0.1");
    cl.addHeader("\nOptions:\n");
    cl.addOption('a', "analyze", "Analyze files. -a prints one grep-friendly summary line per file, -aa prints compact details, -aaa prints detailed nonzero char rows, -aaaa also includes zero-count ASCII/Latin1 rows.");
    cl.addOption('d', "dry-run", "Show what would be fixed without writing files.");
    cl.addOption('r', "recursive", "Recurse into directories. .git directories are ignored.");
    cl.addOption(' ', "fix-latin1", "Convert German Latin1 umlaut bytes (ÄÖÜäöüß) to UTF-8 while preserving already valid UTF-8.");
    cl.addOption(' ', "fix-fuer", "Replace non-alnum-delimited f/F + U+FFFD + r/R with für/Für/FÜR, preserving surrounding boundary bytes.");
    cl.addOption('v', "verbose", "Reserved for future verbose output. Does not change analysis detail; use repeated -a instead.");
    cl.parse(argc, argv);

    if (cl.getArgs().empty())
    {
        cl.error("No files or directories specified.");
    }
    const bool anyFix = cl.isSet("fix-latin1") || cl.isSet("fix-fuer");
    const bool analyze = cl.isSet("analyze") || !anyFix;
    const unsigned analysisLevel = cl.isSet("analyze") ? std::min(cl.getCount("analyze") - 1, 3U) : 0U;
    const std::vector<fs::path> files = collectFiles(cl.getArgs(), cl.isSet("recursive"));
    if (files.empty())
    {
        cl.error("No regular files found.");
    }

    size_t changed = 0;
    for (const fs::path& file: files)
    {
        if (analyze)
        {
            analyzeFile(file, analysisLevel);
        }
        if (anyFix)
        {
            changed += fixFile(file, cl.isSet("fix-latin1"), cl.isSet("fix-fuer"), cl.isSet("dry-run")) ? 1 : 0;
        }
    }
    if (anyFix)
    {
        std::cout << (cl.isSet("dry-run") ? "Would change " : "Changed ") << changed << " file" << (changed == 1 ? "" : "s") << ".\n";
    }
    return 0;
}

int main(int argc, char* argv[])
{
    UNIT_TEST_RUN();
    try
    {
        return realMain(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "fixtext: Error: " << e.what() << "\n";
        return 1;
    }
}
