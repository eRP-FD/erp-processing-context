/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_UTIL_GLOG_HXX
#define E_LIBRARY_UTIL_GLOG_HXX

// #define GLOG_NO_ABBREVIATED_SEVERITIES // for the case of conflict with windows header


// enable logging for iOS and Android debug builds
#if defined(DEBUG) && (defined(__APPLE__) || defined(__ANDROID__) || defined(ANDROID))
    #define ENABLE_DEBUG_LOG 1
#endif

#ifndef ENABLE_DEBUG_LOG
    #define GOOGLE_STRIP_LOG 1    // remove the log-related statements for the levels
#endif

#include <glog/logging.h>


#define LOG_SIMPLE_ENTRY()\
    VLOG(1) << "entering " << __func__

#define LOG_SIMPLE_EXIT()\
    VLOG(1) << "exiting " << __func__

#include "erp/util/GLogConfiguration.hxx"
#define UUID_LOG(expression) LOG(expression) << GLogConfiguration::getProcessUuid() << ": "
#define UUID_VLOG(expression) VLOG(expression) << GLogConfiguration::getProcessUuid() << ": "

/**
 * Please see http://rpg.ifi.uzh.ch/docs/glog.html for more details regarding glog.
 */

 #endif
