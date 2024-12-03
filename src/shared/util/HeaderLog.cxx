/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */
#include "shared/util/HeaderLog.hxx"
#include "shared/util/TLog.hxx"

namespace HeaderLog
{
void error(std::string_view message)
{
    TLOG(ERROR) << message;
}

void error(const std::function<std::ostringstream()>& messageSource)
{
    error(messageSource().view());
}

void warning(std::string_view message)
{
    TLOG(WARNING) << message;
}

void warning(const std::function<std::ostringstream()>& messageSource)
{
    warning(messageSource().view());
}

void info(std::string_view message)
{
    TLOG(INFO) << message;
}

void info(const std::function<std::ostringstream()>& messageSource)
{
    info(messageSource().view());
}

void vlog(int level, std::string_view message)
{
    TVLOG(level) << message;
}

void vlog(int level, const std::function<std::ostringstream()>& messageSource)
{
    vlog(level, messageSource().view());
}

}
