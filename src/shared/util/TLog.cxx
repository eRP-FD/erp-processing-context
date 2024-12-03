#include "shared/util/TLog.hxx"

thread_local std::optional<std::string> tlogContext{};

ScopedLogContext::ScopedLogContext(std::optional<std::string> context)
    : oldLogContext(tlogContext)
{
    tlogContext = std::move(context);
}
ScopedLogContext::~ScopedLogContext() noexcept
{
    tlogContext = std::move(oldLogContext);
}


