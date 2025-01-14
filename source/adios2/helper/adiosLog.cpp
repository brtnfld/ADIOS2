/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * adiosLog.cpp
 *
 *  Created on: Nov 15, 2021
 *      Author: Jason Wang jason.ruonan.wang@gmail.com
 */

#include "adiosLog.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace adios2
{
namespace helper
{

std::string timeColor = "\033[1;36m";
std::string outputColor = "\033[1;32m";
std::string warningColor = "\033[1;33m";
std::string errorColor = "\033[1;31m";
std::string exceptionColor = "\033[1;34m";
std::string defaultColor = "\033[0m";

std::unordered_set<std::string> messages;

void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const LogMode mode)
{
    Log(component, source, activity, message, -1, -1, 0, 0, mode);
}

void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const int priority, const int verbosity, const LogMode mode)
{
    Log(component, source, activity, message, -1, -1, priority, verbosity,
        mode);
}

void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const int logRank, const int commRank, const int priority,
         const int verbosity, const LogMode mode)
{

    // don't print if
    // 1. logRank does not meet commRank, or
    // 2. priority does not meet verbosity, or
    // 3. the same messaage has been already printed
    if ((logRank >= 0 && commRank >= 0 && logRank != commRank) ||
        priority > verbosity || messages.find(message) != messages.end())
    {
        if (mode == LogMode::EXCEPTION)
        {
            throw message;
        }
        else
        {
            return;
        }
    }

    messages.insert(message);

    std::stringstream m;

    auto timeNow =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string timeStr(std::ctime(&timeNow));

    if (timeStr[timeStr.size() - 1] == '\n')
    {
        timeStr.resize(timeStr.size() - 1);
    }

    m << timeColor << "[" << timeStr << "]";

    if (mode == INFO)
    {
        m << outputColor << " [ADIOS2 INFO]" << defaultColor;
    }
    else if (mode == WARNING)
    {
        m << warningColor << " [ADIOS2 WARNING]" << defaultColor;
    }
    else if (mode == ERROR)
    {
        m << errorColor << " [ADIOS2 ERROR]" << defaultColor;
    }
    else if (mode == EXCEPTION)
    {
        m << exceptionColor << " [ADIOS2 EXCEPTION]" << defaultColor;
    }

    if (commRank >= 0)
    {
        m << " [Rank " << commRank << "]";
    }

    m << " <" << component << "> <" << source << "> <" << activity
      << "> : " << message << defaultColor << std::endl;

    if (mode == INFO || mode == WARNING)
    {
        std::cout << m.str();
    }
    else if (mode == ERROR)
    {
        std::cerr << m.str();
    }
    else if (mode == EXCEPTION)
    {
        std::cerr << m.str();
        throw m.str();
    }
}

} // end namespace helper
} // end namespace adios2
