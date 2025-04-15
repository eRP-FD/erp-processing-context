/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_HEADERLOGGING_HXX
#define ERP_PROCESSING_CONTEXT_HEADERLOGGING_HXX

#include <functional>
#include <iosfwd>
#include <string_view>

// use these functions, when the macros are not an option
// used TLOG based macros in implementation
namespace HeaderLog
{
/**
 * since inclusion of tee 3 protocol code from ePA we have two logging implementations that may clash.
 * to allow logging from headers without inclusing any macro based logging framework
 * these functions can be used to call the eRP macros.
 **/

// equivalent to: TLOG(ERROR) << message;
void error(std::string_view message);

// equivalent to: TLOG(ERROR) << messageSource().view();
void error(const std::function<std::ostringstream()>& messageSource);

// equivalent to: TLOG(WARNING) << message;
void warning(std::string_view message);

// equivalent to: TLOG(WARNING) << messageSource().view();
void warning(const std::function<std::ostringstream()>& messageSource);

// equivalent to: TLOG(INFO) << message;
void info(std::string_view message);

// equivalent to: TLOG(INFO) << messageSource().view();
void info(const std::function<std::ostringstream()>& messageSource);

// equivalent to: TVLOG(level) << message;
void vlog(int level, std::string_view message);

// equivalent to: TVLOG(level) << messageSource().view();
void vlog(int level, const std::function<std::ostringstream()>& messageSource);
}


#endif// ERP_PROCESSING_CONTEXT_HEADERLOGGING_HXX
