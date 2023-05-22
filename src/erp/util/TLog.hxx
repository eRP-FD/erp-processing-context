/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_TLOG_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_TLOG_HXX

#include "erp/util/GLog.hxx"

#include "erp/util/ThreadNames.hxx"
#include "erp/server/Worker.hxx"

class ScopedLogContext
{
public:
    explicit ScopedLogContext(std::optional<std::string> context);
    ~ScopedLogContext() noexcept;

private:
    std::optional<std::string> oldLogContext;
};

#define TLOG(level) LOG(level) << ThreadNames::instance().getCurrentThreadName() << "/" << erp::server::Worker::tlogContext.value_or("") << ": "
#ifdef ENABLE_DEBUG_LOG
#define TVLOG(level) VLOG(level) << "v=" << (level) << " " << ThreadNames::instance().getCurrentThreadName() << "/" << erp::server::Worker::tlogContext.value_or("") << ": "
#else
#define TVLOG(level) static_cast<void>(level), LOG_IF(INFO, false)
#endif

#endif
