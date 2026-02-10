/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "WSViewService.hxx"

#include "ErpFhirWS.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/repository/views/VersionMappingView.hxx"
#include "fhirtools/repository/views/CodeCachingView.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Configuration.hxx"

namespace erp_fhir_ws
{
namespace
{
std::optional<model::Timestamp> toDeTimestamp(std::optional<date::local_days> ts)
{
    return ts ? std::optional{model::Timestamp{model::Timestamp::GermanTimezone, *ts}} : std::nullopt;
}
}

template<typename T>
std::string_view WSViewService::View::Cmp::id(const T& value)
{
    return value;
}
std::string_view WSViewService::View::Cmp::id(const View& view)
{
    return view.id;
}

template<typename RhsT, typename LhsT>
bool WSViewService::View::Cmp::operator()(const LhsT& lhs, const RhsT& rhs) const
{
    return id(lhs) < id(rhs);
}

boost::asio::execution_context::id WSViewService::id;

WSViewService::WSViewService(boost::asio::execution_context& context)
    : service{context}
{
}

void WSViewService::shutdown()
{
}

const WSViewService::View& WSViewService::getView(std::string_view viewParam)
{
    {
        std::shared_lock lock{mViewsMutex};
        auto view = mViews.find(viewParam);
        if (view != mViews.end())
        {
            return *view;
        }
    }
    std::unique_lock lock{mViewsMutex};
    auto view = mViews.find(viewParam);
    return (view != mViews.end()) ? *view : createView(viewParam);
}

const WSViewService::View& WSViewService::createView(std::string_view viewParam)
{
    using namespace std::string_literals;
    const auto& config = Configuration::instance();
    std::list<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceViewConfiguration::ViewConfig>>> viewConfigs;
    size_t colonPos = viewParam.find(':');
    Expect3(colonPos != std::string_view::npos && colonPos > 0, "No process type in view name: "s.append(viewParam),
            ErpFhirWsException);
    auto processType = viewParam.substr(0, colonPos);
    if (processType == "PC")
    {
        viewConfigs = config.fhirResourceViewConfiguration<Configuration::ERP>().allViews();
    }
    else if (processType == "MX")
    {
        viewConfigs = config.fhirResourceViewConfiguration<Configuration::MedicationExporter>().allViews();
    }
    else
    {
        Fail2("Unknown process Type in: "s.append(viewParam), ErpFhirWsException);
    }
    auto viewName = viewParam.substr(colonPos + 1);
    auto viewConf =
        std::ranges::find(viewConfigs, viewName, &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
    Expect3(viewConf != viewConfigs.end(),
            "No such view in process "s.append(processType).append(": ").append(viewName), ErpFhirWsException);
    const auto* backend = std::addressof(Fhir::instance().backend());
    auto fhirView = (*viewConf)->view(backend);
    fhirView = fhirtools::VersionMappingView::create(std::string{viewParam} + "-ver"s,
                                                     fhirtools::VersionMapper{config.fhirVersionMapping()}, fhirView);
    fhirView = fhirtools::CodeCachingView::create(std::string{viewParam} + "-cached"s, fhirView);
    return *mViews
                .emplace(std::string{viewParam}, toDeTimestamp((*viewConf)->mStart), toDeTimestamp((*viewConf)->mEnd),
                         fhirView)
                .first;
}


}
