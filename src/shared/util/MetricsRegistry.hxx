// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/util/DurationCategory.hxx"

#include <boost/core/noncopyable.hpp>
#include <gsl/gsl-lite.hpp>
#include <prometheus/family.h>
#include <chrono>

namespace prometheus
{
class Histogram;
class Registry;
}

class MetricsRegistry : boost::noncopyable
{
public:
    static MetricsRegistry& instance();

    ~MetricsRegistry();

    // counts (observes), duration, category, metric in prometheus histogram.
    void count(const std::chrono::steady_clock::duration& duration, DurationCategory category,
               const std::string& metric);

    std::string serialize() const;

    void clear();

private:
    explicit MetricsRegistry();
    prometheus::Family<prometheus::Histogram>* buildHistogram();

    gsl::not_null<std::unique_ptr<prometheus::Registry>> mPrometheusRegistry;
    gsl::not_null<prometheus::Family<prometheus::Histogram>*> mHistogram;
};
