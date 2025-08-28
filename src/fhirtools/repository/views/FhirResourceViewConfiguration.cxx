/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"
#include "fhirtools/repository/views/ConfigurationHelper.hxx"
#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/views/KbvSchluesseltabellenConfiguration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"

#include <rapidjson/pointer.h>
#include <ranges>

namespace fhirtools
{

namespace
{
std::string periodToString(const std::optional<date::local_days>& start, const std::optional<date::local_days>& end,
                           const date::days& offset)
{
    if (start && end)
    {
        return date::format("%Y-%m-%d", (*start + offset))
            .append(" - ")
            .append(date::format("%Y-%m-%d", (*end + offset)));
    }
    if (start)
    {
        return date::format("%Y-%m-%d", (*start + offset)) + " - none";
    }
    if (end)
    {
        return "none - " + date::format("%Y-%m-%d", (*end + offset));
    }
    return "none - none";
}
std::optional<date::local_days> earlier(const std::optional<date::local_days>& lhs,
                                        const std::optional<date::local_days>& rhs)
{
    if (lhs && rhs)
    {
        return *lhs < *rhs ? *lhs : *rhs;
    }
    return lhs ? lhs : rhs;
}
std::optional<date::local_days> later(const std::optional<date::local_days>& lhs,
                                      const std::optional<date::local_days>& rhs)
{
    return std::max(lhs, rhs);
}
}

FhirResourceViewConfiguration::ViewConfigInternal::ViewConfigInternal(std::string id,
                                                                      std::optional<date::local_days> start,
                                                                      std::optional<date::local_days> end,
                                                                      std::set<std::string> groups, std::string info)
    : ViewConfig{std::move(id), start, end, std::move(groups), std::move(info)}
{
}

gsl::not_null<std::shared_ptr<FhirStructureRepositoryView>> fhirtools::FhirResourceViewConfiguration::ViewConfig::view(
    gsl::not_null<const FhirStructureRepositoryBackend*> backend) const
{
    const auto& allGroups = backend->allGroups();
    std::set<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceGroup>>> groups;
    for (const auto& groupId : mGroups)
    {
        auto group = allGroups.find(groupId);
        FPExpect3(group != allGroups.end(), "group not found in backend: " + groupId, std::logic_error);
        groups.emplace(group->second);
    }
    return fhirtools::FhirResourceViewGroupSet::create(mId, std::move(groups), backend);
}


FhirResourceViewConfiguration::FhirResourceViewConfiguration(
    const FhirResourceGroupResolver& resolver, const gsl::not_null<const rapidjson::Value*>& fhirResourceViews,
    const KbvSchluesseltabellenConfiguration& kbvSchluesseltabellenConfiguration, const date::days& globalOffset)
        : mKbvSchluesseltabellenConfiguration(kbvSchluesseltabellenConfiguration)
{
    using namespace std::string_literals;
    FPExpect(fhirResourceViews->IsArray(), "fhir-resource-views is not an array");
    const rapidjson::Pointer ptValidFrom{"/valid-from"};
    const rapidjson::Pointer ptValidUntil{"/valid-until"};

    if (globalOffset != date::days{0})
    {
        TLOG(WARNING) << "adding " << globalOffset << " days GLOBAL OFFSET to all FHIR resource views";
    }

    for (const auto& item : fhirResourceViews->GetArray())
    {
        const auto id = ConfigurationHelper::retrieveId(item);

        auto start = ConfigurationHelper::retrieveDate(ptValidFrom, item);
        auto end = ConfigurationHelper::retrieveDate(ptValidUntil, item);

        std::set<std::string> groups = ConfigurationHelper::retrieveGroups(item, resolver, id);

        auto schluesselTabellen = kbvSchluesseltabellenConfiguration.entriesWithin(start, end);
        FPExpect(! schluesselTabellen.empty(),
                 "no KBV Schluesseltabellen found for period "s.append(periodToString(start, end, date::days{0})));

        bool first = true;
        for (auto&& schluesselTabelle : schluesselTabellen)
        {
            const auto info = std::string{id}
                                  .append(" (")
                                  .append(periodToString(start, end, globalOffset))
                                  .append(") merged with ")
                                  .append(schluesselTabelle.id)
                                  .append(" (")
                                  .append(periodToString(schluesselTabelle.start, schluesselTabelle.end, globalOffset))
                                  .append(")");
            const auto mergedId = first ? id : std::string{id}.append("_").append(schluesselTabelle.id);
            auto mergedGroups = groups;
            mergedGroups.merge(std::move(schluesselTabelle.mGroups));
            auto [mergedStart, mergedEnd] = mergePeriod(start, end, schluesselTabelle.start, schluesselTabelle.end);

            if (mergedStart && globalOffset != date::days{0})
            {
                *mergedStart += globalOffset;
            }
            if (mergedEnd && globalOffset != date::days{0})
            {
                *mergedEnd += globalOffset;
            }

            addView(mergedStart, mergedEnd, mergedGroups, mergedId, info);
            first = false;
        }
    }
    check();
}

FhirResourceViewConfiguration::ViewList
FhirResourceViewConfiguration::getViewInfo(const date::local_days& validDate) const
{
    decltype(getViewInfo(validDate)) result;

    for (auto view = mResourceViews.lower_bound(validDate); view != mResourceViews.end() && view->first >= validDate;
         ++view)
    {
        if (! view->second->mStart || *view->second->mStart <= validDate)
        {
            result.emplace_back(view->second);
        }
    }
    return result;
}

FhirResourceViewConfiguration::ViewList FhirResourceViewConfiguration::getViewInfo(const Date& validDate) const
{
    return getViewInfo(date::local_days{date::year_month_day{validDate}});
}

std::list<gsl::not_null<std::shared_ptr<const FhirResourceViewConfiguration::ViewConfig>>>
FhirResourceViewConfiguration::allViews() const
{
    decltype(allViews()) result;
    std::ranges::transform(mResourceViews, std::back_inserter(result), [](const auto& v) {
        return v.second;
    });
    return result;
}

const KbvSchluesseltabellenConfiguration& FhirResourceViewConfiguration::kbvSchluesseltabellenConfiguration() const
{
    return mKbvSchluesseltabellenConfiguration;
}

std::tuple<std::optional<date::local_days>, std::optional<date::local_days>> FhirResourceViewConfiguration::mergePeriod(
    std::optional<date::local_days> viewStart, std::optional<date::local_days> viewEnd,
    std::optional<date::local_days> schluesselTabelleStart, std::optional<date::local_days> schluesselTabelleEnd)
{
    auto mergedStart = later(viewStart, schluesselTabelleStart);
    auto mergedEnd = earlier(viewEnd, schluesselTabelleEnd);
    return {mergedStart, mergedEnd};
}

void FhirResourceViewConfiguration::addView(std::optional<date::local_days> start, std::optional<date::local_days> end,
                                            const std::set<std::string>& groups, const std::string& id,
                                            const std::string& info)
{
    using namespace date::literals;
    const auto it = mResourceViews.emplace(
        std::piecewise_construct, std::forward_as_tuple(end.value_or(date::local_days::max())),
        std::forward_as_tuple(std::make_shared<ViewConfigInternal>(id, start, end, groups, info)));
    TVLOG(2) << "view " << *it->second;
}

void FhirResourceViewConfiguration::check() const
{
    FPExpect3(! mResourceViews.empty(), "no view configured", std::logic_error);
    auto prev = mResourceViews.begin();
    FPExpect(prev->second->isConsistent(), (std::ostringstream{} << "inconsistent view: " << *prev->second).str());
    for (auto end = mResourceViews.end(), view = next(prev); view != end; prev = view, ++view)
    {
        FPExpect(view->second->isConsistent(), (std::ostringstream{} << "inconsistent view: " << view->second).str());
    }
}

bool FhirResourceViewConfiguration::ViewConfigInternal::isConsistent() const
{
    if (mStart && mEnd)
    {
        return *mStart <= *mEnd;
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const FhirResourceViewConfiguration::ViewConfig& view)
{
    os << view.mId;
    os << " (" << periodToString(view.mStart, view.mEnd, date::days{0}) << ")";
    os << " Info: " << view.mInfo;
    return os;
}

}
