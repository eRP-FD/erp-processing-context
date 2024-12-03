/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_WRAPPERS_GLOG_HXX
#define EPA_LIBRARY_WRAPPERS_GLOG_HXX

#include "library/log/Location.hxx"
#include "library/wrappers/detail/DisableWarnings.hxx"


// To avoid conflicts with macros in windows.h
#ifndef GLOG_NO_ABBREVIATED_SEVERITIES
#define GLOG_NO_ABBREVIATED_SEVERITIES
#endif

// GEMREQ-start A_21195
#ifndef ENABLE_LOG
// If logging is not explicitly enabled, strip everything (safe default).
#undef GOOGLE_STRIP_LOG 4
#define GOOGLE_STRIP_LOG 4 // 4 > FATAL (= 3)
#endif
// GEMREQ-end A_21195

DISABLE_WARNINGS_BEGIN

#ifdef _WIN32
// GLog after version 0.4.0 indirectly includes windows.h. To avoid a bug in files that include this
// header and also winsock2.h, we have to include the latter file first.
// Ref: https://stackoverflow.com/a/23294767
#include <winsock2.h>
#endif

#include <glog/logging.h>

// windows.h also defines a macro called DELETE which clashes with enum cases in our code -> #undef.
#ifdef DELETE
#undef DELETE
#endif

DISABLE_WARNINGS_END

#endif
