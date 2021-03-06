/** -*-c++-*-
 *
 *  Copyright 2015 The Australian National University
 *
 *  stringUtils.hpp
 *
 *  Some functions for processing strings.
 *
 *  Olaf Delgado-Friedrichs jun 15
 *
 */

#ifndef ANU_AM_STRINGUTILS_HPP
#define ANU_AM_STRINGUTILS_HPP

#include <string>

namespace anu_am
{
namespace stringutils
{

bool startsWith(std::string const str, std::string const prefix)
{
    return str.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string const str, std::string const prefix)
{
    return str.substr(str.size() - prefix.size()) == prefix;
}

std::string stripLeading(std::string const str, std::string const prefix)
{
    if (startsWith(str, prefix))
        return str.substr(prefix.size());
    else
        return str;
}

std::vector<std::string> split(std::string const str, char const sep)
{
    std::vector<std::string> result;

    size_t pos = 0;
    while (pos < str.size())
    {
        size_t next = str.find(sep, pos);
        if (next == std::string::npos)
            next = str.size();

        result.push_back(str.substr(pos, next - pos));
        pos = next + 1;
    }

    return result;
}

} // namespace stringutils
} // namespace anu_am

#endif // ANU_AM_STRINGUTILS_HPP
