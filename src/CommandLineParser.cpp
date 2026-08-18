// Command line parser.
//
// Copyright (c) 2021-2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#include "CommandLineParser.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace ut1
{

#define HEADER_PREFIX "header:"

CommandLineParser::Option& CommandLineParser::Option::listOption()
{
    isList = true;
    return *this;
}

CommandLineParser::Option& CommandLineParser::Option::addAlias(char alias)
{
    shortOptionAlias = alias;
    parent->shortOptionToLongOption[shortOptionAlias] = longOption;
    return *this;
}

void CommandLineParser::Option::setValue(const std::string& v, char listSepChar)
{
    if (isList && (count > 0))
    {
        value += listSepChar;
        value += v;
    }
    else
    {
        value = v;
    }
    count++;
    if (argName.empty())
    {
        value = std::to_string(count);
    }
}

size_t CommandLineParser::Option::getHelpNameLen() const
{
    return longOption.length() + argName.length() + (!argName.empty());
}

CommandLineParser::CommandLineParser(const std::string& programName_, const std::string& usage_, const std::string& footer_, const std::string& version_)
: programName(programName_)
, usage(usage_)
, footer(footer_)
, version(version_)
{
    addOption(' ', "version", "Print version and exit.");
    addOption('h', "help", "Print this help message and exit.");
}

void CommandLineParser::addHeader(const std::string& header)
{
    optionList.insert(optionList.end() - std::min(optionList.size(), size_t(2)), HEADER_PREFIX + header);
}

CommandLineParser::Option& CommandLineParser::addOption(char shortOption, const std::string& longOption, const std::string& help, const std::string& argName, const std::string& defaultValue)
{
    if (shortOption == ' ')
    {
        shortOption = 0;
    }
    if (options.count(longOption))
    {
        throw std::runtime_error("Option --" + longOption + " already exists.");
    }
    if (shortOption && getShortOption(shortOption))
    {
        throw std::runtime_error(std::string("Option -") + shortOption + " already exists.");
    }

    Option opt;
    opt.shortOption = shortOption;
    opt.longOption = longOption;
    opt.help = help;
    opt.argName = argName;
    opt.defaultValue = defaultValue;
    opt.value = defaultValue;
    opt.parent = this;
    options[longOption] = opt;
    if (shortOption)
    {
        shortOptionToLongOption[shortOption] = longOption;
    }
    optionList.insert(optionList.end() - std::min(optionList.size(), size_t(2)), longOption);
    return options[longOption];
}

void CommandLineParser::parse(int argc, const char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == '-')
            {
                if (argv[i][2] == 0)
                {
                    for (++i; i < argc; i++)
                    {
                        args.emplace_back(argv[i]);
                    }
                    break;
                }
                parseLongOption(argc, argv, i);
            }
            else if (argv[i][1] == 0)
            {
                args.emplace_back(argv[i]);
            }
            else
            {
                parseShortOptions(argc, argv, i);
            }
        }
        else
        {
            args.emplace_back(argv[i]);
        }
    }
    if (isSet("help"))
    {
        printMessage(getUsageStr());
        std::exit(0);
    }
    if (isSet("version"))
    {
        printMessage("version " + version + " (" + getCompileDate() + ")");
        std::exit(0);
    }
}

unsigned CommandLineParser::getCount(const std::string& longOption) const
{
    const Option* option = getOption(longOption);
    if (!option)
    {
        throw std::runtime_error("Unknown option --" + longOption + ".");
    }
    return option->count;
}

const std::string& CommandLineParser::getStr(const std::string& longOption) const
{
    const Option* option = getOption(longOption);
    if (!option)
    {
        throw std::runtime_error("Unknown option --" + longOption + ".");
    }
    return option->value;
}

void CommandLineParser::error(const std::string& message, int exitStatus) const
{
    printMessage("Error: " + message);
    std::exit(exitStatus);
}

void CommandLineParser::printMessage(const std::string& message) const
{
    std::cout << programName << ": " << message << "\n";
}

std::string CommandLineParser::getUsageStr() const
{
    std::stringstream ret;
    ret << joinStrings(splitLines(usage, 80), "\n") << "\n";
    size_t maxHelpNameLen = 0;
    for (const auto& opt: options)
    {
        maxHelpNameLen = std::max(maxHelpNameLen, opt.second.getHelpNameLen());
    }
    const size_t helpStartCol = maxHelpNameLen + 8;
    const size_t helpWrapCol = 80;
    for (const auto& name: optionList)
    {
        if (hasPrefix(name, HEADER_PREFIX))
        {
            ret << name.substr(strlen(HEADER_PREFIX));
            continue;
        }
        const Option* option = getOption(name);
        ret << "  ";
        if (option->shortOption)
        {
            ret << "-" << option->shortOption;
        }
        else
        {
            ret << "  ";
        }
        std::string nameEqArg = option->longOption;
        if (!option->argName.empty())
        {
            nameEqArg += "=" + option->argName;
        }
        ret << " --" << nameEqArg;
        ret << std::string(std::max(size_t(0), maxHelpNameLen - nameEqArg.length()) + 1, ' ');
        std::vector<std::string> lines = splitLines(option->help, helpWrapCol);
        ret << joinStrings(lines, "\n" + std::string(helpStartCol, ' '));
        if (!option->defaultValue.empty())
        {
            ret << " (default=" << option->defaultValue << ")";
        }
        ret << "\n";
    }
    ret << footer;
    std::string u = ret.str();
    replaceStringInPlace(u, "$programName", programName);
    replaceStringInPlace(u, "$version", version);
    replaceStringInPlace(u, "$compileDate", getCompileDate());
    return u;
}

CommandLineParser::Option* CommandLineParser::getOption(const std::string& longOption)
{
    auto it = options.find(longOption);
    return (it == options.end()) ? nullptr : &(it->second);
}

const CommandLineParser::Option* CommandLineParser::getOption(const std::string& longOption) const
{
    return const_cast<CommandLineParser*>(this)->getOption(longOption);
}

CommandLineParser::Option* CommandLineParser::getShortOption(char shortOption)
{
    auto it = shortOptionToLongOption.find(shortOption);
    return (it == shortOptionToLongOption.end()) ? nullptr : getOption(it->second);
}

void CommandLineParser::parseLongOption(int argc, const char* argv[], int& i)
{
    std::vector<std::string> fields = splitString(argv[i] + 2, '=', 1);
    std::string longOption = fields[0];
    Option* option = getOption(longOption);
    if (!option)
    {
        std::vector<std::string> matches;
        for (const auto& optName: optionList)
        {
            if (hasPrefix(optName, longOption) && !hasPrefix(optName, HEADER_PREFIX))
            {
                matches.push_back(optName);
            }
        }
        if (matches.size() == 1)
        {
            longOption = matches[0];
            option = getOption(longOption);
        }
        else if (matches.empty())
        {
            error("Unknown option --" + longOption + ".");
        }
        else
        {
            error("Ambiguous option --" + longOption + ".");
        }
    }
    assert(option);
    if (option->hasArg())
    {
        if (fields.size() == 2)
        {
            option->setValue(fields[1], listSepChar);
        }
        else if (++i < argc)
        {
            option->setValue(argv[i], listSepChar);
        }
        else
        {
            error("Option --" + longOption + " requires an argument.");
        }
    }
    else if (fields.size() == 1)
    {
        option->setValue("", 0);
    }
    else
    {
        error("Option --" + longOption + " does not accept arguments.");
    }
}

void CommandLineParser::parseShortOptions(int argc, const char* argv[], int& i)
{
    for (int j = 1; argv[i][j]; j++)
    {
        Option* option = getShortOption(argv[i][j]);
        if (!option)
        {
            error(std::string("Unknown option -") + argv[i][j] + ".");
        }
        if (option->hasArg())
        {
            if (argv[i][j + 1])
            {
                option->setValue(argv[i] + j + 1, listSepChar);
            }
            else if (++i < argc)
            {
                option->setValue(argv[i], listSepChar);
            }
            else
            {
                error("Option --" + option->longOption + " requires an argument.");
            }
            break;
        }
        option->setValue("", 0);
    }
}

} // namespace ut1
