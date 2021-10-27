/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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

#define TLOG(level) LOG(level) << ThreadNames::instance().getCurrentThreadName() << '/' << erp::server::Worker::finishedTaskCount << "/" << erp::server::Worker::tlogContext.value_or("") << ": "
#define TVLOG(level) VLOG(level) << ThreadNames::instance().getCurrentThreadName() << '/' << erp::server::Worker::finishedTaskCount << "/" << erp::server::Worker::tlogContext.value_or("") << ": "

#endif
