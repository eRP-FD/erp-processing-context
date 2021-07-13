#ifndef ERP_PROCESSING_CONTEXT_UTIL_TLOG_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_TLOG_HXX

#include "erp/util/GLog.hxx"

#include "erp/util/ThreadNames.hxx"


#define TLOG(level) LOG(level) << ThreadNames::instance().getCurrentThreadName() << ": "
#define TVLOG(level) VLOG(level) << ThreadNames::instance().getCurrentThreadName() << ": "

#endif
