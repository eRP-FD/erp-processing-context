// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/util/MetricsRegistry.hxx"
#include "shared/util/TLog.hxx"

#include <magic_enum/magic_enum.hpp>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

MetricsRegistry& MetricsRegistry::instance()
{
    static MetricsRegistry theInstance;
    return theInstance;
}

MetricsRegistry::~MetricsRegistry() = default;

void MetricsRegistry::count(const std::chrono::steady_clock::duration& duration, DurationCategory category,
                            const std::string& metric)
{
    static const std::vector<double> bucketBoundaries{1 / 1000.0, 5 / 1000.0, 10 / 1000.0, 50 / 1000.0};
    try
    {
        mHistogram
            ->Add({{"category", std::string{magic_enum::enum_name(category)}}, {"metric", metric}}, bucketBoundaries)
            .Observe(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) /
                     1000.0);
    }
    catch (const std::exception& ex)
    {
        TLOG(WARNING) << "exception during recording of metric " << metric << ": " << ex.what();
    }
}

std::string MetricsRegistry::serialize() const
{
    return prometheus::TextSerializer{}.Serialize(mHistogram->Collect());
}

void MetricsRegistry::clear()
{
    mPrometheusRegistry->Remove(*mHistogram);
    mHistogram = buildHistogram();
}

MetricsRegistry::MetricsRegistry()
    : mPrometheusRegistry(std::make_unique<prometheus::Registry>(prometheus::Registry::InsertBehavior::Throw))
    , mHistogram(buildHistogram())
{
}

prometheus::Family<prometheus::Histogram>* MetricsRegistry::buildHistogram()
{
    return &prometheus::BuildHistogram()
                .Name("backend_duration_seconds")
                .Help("Backend call duration in seconds")
                .Register(*mPrometheusRegistry);
}
