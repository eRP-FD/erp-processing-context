/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOGGING_HXX
#define EPA_LIBRARY_LOG_LOGGING_HXX

#include "library/wrappers/GLog.hxx"

/**
 * Please see http://rpg.ifi.uzh.ch/docs/glog.html for more details regarding glog.
 */

#include "library/log/Log.hxx"
#include "library/log/LogTrace.hxx"

/**
 * Remove the definitions of LOG and VLOG made by the GLOG framework and replace them
 * with our own so that we
 * - can use additional log levels INFO1 to INFO4 and DEBUG0 to DEBUG4
 * - switch log format to JSON and
 * - get support for classified text.
 */
#include "library/log/LogMacros.hxx"


#endif
