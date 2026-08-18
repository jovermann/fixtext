// Command line parser.
//
// Copyright (c) 2021-2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#pragma once

#include "MiscUtils.hpp"
#include <map>
#include <string>
#include <vector>

namespace ut1
{

class CommandLineParser
{
public:
    struct Option
    {
        bool hasArg() const noexcept { return !argName.empty(); }
        void setValue(const std::string& v, char listSepChar);
        size_t getHelpNameLen() const;

        char shortOption{};
        char shortOptionAlias{};
        std::string longOption;
        std::string help;
        std::string argName;
        std::string defaultValue;
        bool isList{};
        std::string value;
        unsigned count{};
        CommandLineParser *parent{};

        Option& listOption();
        Option& addAlias(char alias);
    };

    CommandLineParser(const std::string& programName_, const std::string& usage_, const std::string& footer_, const std::string& version_);
    void addHeader(const std::string& header);
    Option& addOption(char shortOption, const std::string& longOption, const std::string& help, const std::string& argName = std::string(), const std::string& defaultValue = std::string());
    void parse(int argc, const char* argv[]);
    void parse(int argc, char* argv[]) { parse(argc, const_cast<const char **>(argv)); }
    bool isSet(const std::string& longOption) const { return getCount(longOption) > 0; }
    unsigned getCount(const std::string& longOption) const;
    const std::string& getStr(const std::string& longOption) const;
    const std::vector<std::string>& getArgs() const noexcept { return args; }
    [[noreturn]] void error(const std::string& message, int exitStatus = 1) const;
    void printMessage(const std::string& message) const;
    std::string getUsageStr() const;

private:
    Option* getOption(const std::string& longOption);
    const Option* getOption(const std::string& longOption) const;
    Option* getShortOption(char shortOption);
    void parseLongOption(int argc, const char* argv[], int& i);
    void parseShortOptions(int argc, const char* argv[], int& i);

    std::map<std::string, Option> options;
    std::vector<std::string> optionList;
    std::map<char,std::string> shortOptionToLongOption;
    std::vector<std::string> args;
    const char listSepChar{'\1'};
    std::string programName;
    std::string usage;
    std::string footer;
    std::string version;
};

} // namespace ut1
