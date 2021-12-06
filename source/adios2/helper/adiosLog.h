/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * adiosLog.h
 *
 *  Created on: Nov 15, 2021
 *      Author: Jason Wang jason.ruonan.wang@gmail.com
 */

#include <string>

namespace adios2
{
namespace helper
{

enum LogMode : char
{
    EXCEPTION = 'x',
    ERROR = 'e',
    WARNING = 'w',
    INFO = 'i'
};

/**
 * Print outputs, warnings, errors, and exceptions
 * @param component: Engine, Transport, Operator, etc.
 * @param source: class name of component
 * @param activity: function name where this is called
 * @param message: text message
 * @param mode: INFO, WARNING, ERROR, or EXCEPTION
 */
void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const LogMode mode);

/**
 * Print outputs, warnings, errors, and exceptions
 * @param component: Engine, Transport, Operator, etc.
 * @param source: class name of component
 * @param activity: function name where this is called
 * @param message: text message
 * @param priority: only print if(priority<=verbosity)
 * @param verbosity: engine parameter for engine wide verbosity level
 * @param mode: INFO, WARNING, ERROR, or EXCEPTION
 */
void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const int priority, const int verbosity, const LogMode mode);

/**
 * Print outputs, warnings, errors, and exceptions
 * @param component: Engine, Transport, Operator, etc.
 * @param source: class name of component
 * @param activity: function name where this is called
 * @param message: text message
 * @param logRank: only print if(logRank==commRank)
 * @param commRank: current MPI rank
 * @param priority: only print if(priority<=verbosity)
 * @param verbosity: engine parameter for engine wide verbosity level
 * @param mode: INFO, WARNING, ERROR, or EXCEPTION
 */
void Log(const std::string &component, const std::string &source,
         const std::string &activity, const std::string &message,
         const int logRank, const int commRank, const int priority,
         const int verbosity, const LogMode mode);

} // end namespace helper
} // end namespace adios2
