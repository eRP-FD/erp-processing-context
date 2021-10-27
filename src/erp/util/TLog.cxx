#include "erp/util/TLog.hxx"


ScopedLogContext::ScopedLogContext(std::optional<std::string> context)
    : oldLogContext(erp::server::Worker::tlogContext)
{
    erp::server::Worker::tlogContext = std::move(context);
}
ScopedLogContext::~ScopedLogContext() noexcept
{
    erp::server::Worker::tlogContext = std::move(oldLogContext);
}


