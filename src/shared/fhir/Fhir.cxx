/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Fhir.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Configuration.hxx"
#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"

#include <fhirtools/repository/FhirResourceViewVerifier.hxx>
#include <algorithm>

std::unique_ptr<Fhir> Fhir::mInstance;

template<config::ProcessType ProcessT>
class FhirImpl final : public Fhir
{
public:

    FhirImpl();
    ~FhirImpl() override = default;

    const FhirConverter& converter() const override
    {
        return mConverter;
    }

    fhirtools::FhirResourceViewConfiguration::ViewList
    structureRepository(const model::Timestamp& referenceTimestamp) const override;

    const fhirtools::FhirStructureRepositoryBackend& backend() const override
    {
        return mBackend;
    }

    fhirtools::FhirResourceViewConfiguration fhirResourceViewConfiguration() const override;

    void ensureInitialized() override;

    void load();
    void validateViews() const;
    void validateProfileRequirements(const fhirtools::FhirResourceViewConfiguration::ViewList& viewList,
                                     const model::Timestamp& timestamp) const;

    FhirConverter mConverter;
    fhirtools::FhirStructureRepositoryBackend mBackend;
    std::once_flag initialized;
};

template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::ensureInitialized()
{
    std::call_once(initialized, [&]{
        load();
        validateViews();
    });
}

template<config::ProcessType ProcessT>
FhirImpl<ProcessT>::FhirImpl()
    : mConverter()
    , mBackend()
{
}
template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::load()
{
    const auto& config = Configuration::instance();
    auto filesAsString = config.getArray(ProcessT::FHIR_STRUCTURE_DEFINITIONS);
    std::list<std::filesystem::path> files;
    std::transform(filesAsString.begin(), filesAsString.end(), std::back_inserter(files), [](const auto& str) {
        return std::filesystem::path{str};
    });
    TLOG(INFO) << "Loading FHIR structure repository.";
    auto resolver = config.fhirResourceGroupConfiguration<ProcessT>();
    for (const auto& [url, version] : config.synthesizeCodesystem<ProcessT>())
    {
        mBackend.synthesizeCodeSystem(url, version, resolver);
    }
    for (const auto& [url, version] : config.synthesizeValuesets<ProcessT>())
    {
        mBackend.synthesizeValueSet(url, version, resolver);
    }
    mBackend.load(files, resolver);
}

template<config::ProcessType ProcessT>
fhirtools::FhirResourceViewConfiguration::ViewList
FhirImpl<ProcessT>::structureRepository(const model::Timestamp& referenceTimestamp) const
{
    const auto& config = Configuration::instance();

    auto viewConfig = config.fhirResourceViewConfiguration<ProcessT>();
    fhirtools::FhirResourceViewConfiguration::ViewList viewList;
    const auto& referenceDate = fhirtools::Date{referenceTimestamp.toGermanDate()};
    viewList = viewConfig.getViewInfo(referenceDate);
    Expect(! viewList.empty(), "invalid reference date: " + referenceDate.toString(false));
    return viewList;
}


template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::validateViews() const
{

    const auto& config = Configuration::instance();
    auto viewConfig = config.fhirResourceViewConfiguration<ProcessT>();
    std::set<date::local_days> viewDates;
    for (const auto& viewConf : viewConfig.allViews())
    {
        auto view = viewConf->view(&mBackend);
        fhirtools::FhirResourceViewVerifier verifier{mBackend, view.get().get()};
        verifier.verify();
        if (viewConf->mStart)
        {
            viewDates.emplace(*viewConf->mStart);
        }
        if (viewConf->mEnd)
        {
            viewDates.emplace(*viewConf->mEnd);
        }
    }

    for (const auto& date : viewDates)
    {
        model::Timestamp timestamp{model::Timestamp::GermanTimezone, date};
        auto viewList = structureRepository(timestamp);
        validateProfileRequirements(viewList, timestamp);
    }
}

template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::validateProfileRequirements(const fhirtools::FhirResourceViewConfiguration::ViewList& viewList,
                                                     const model::Timestamp& timestamp) const
{
    using ViewConfig = fhirtools::FhirResourceViewConfiguration::ViewConfig;
    for (const auto& profileTypeReq : ProcessT::requiredProfiles)
    {
        const auto onlyViews = [&](const std::string& id) -> bool {
            return profileTypeReq.second.onlyViews.contains(id);
        };
        if (! profileTypeReq.second.onlyViews.empty() && std::ranges::none_of(viewList, onlyViews, &ViewConfig::mId))
        {
            continue;
        }
        if (profileTypeReq.first != model::ProfileType::fhir)
        {
            std::optional<fhirtools::DefinitionKey> key;
            for (const auto& viewConf : viewList)
            {
                auto view = viewConf->view(&mBackend);
                key = profileWithVersion(profileTypeReq.first, *view);
                if (key)
                {
                    TVLOG(2) << view->id() << " found profile: " << *key;
                    break;
                }
            }
            Expect3(key.has_value(),
                    "could not resolve " + std::string{magic_enum::enum_name(profileTypeReq.first)} +
                        " for date: " + timestamp.toGermanDate(),
                    std::logic_error);
        }
    }
}


template<config::ProcessType ProcessT>
fhirtools::FhirResourceViewConfiguration FhirImpl<ProcessT>::fhirResourceViewConfiguration() const
{
    return Configuration::instance().fhirResourceViewConfiguration<ProcessT>();
}

const Fhir& Fhir::instance()
{
    Expect3(mInstance != nullptr, "Attempt to call Fhir::instance before init", std::logic_error);
    mInstance->ensureInitialized();
    return *mInstance;
}

template<config::ProcessType ProcessT>
void Fhir::init(Init init)
{
    using InstanceType = FhirImpl<ProcessT>;
    if (mInstance)
    {
        const auto& instance = *mInstance;
        Expect3(dynamic_cast<InstanceType*>(mInstance.get()),
                std::string{"Fhir already initialized with: "} + typeid(instance).name(), std::logic_error);
        return;
    }
    mInstance = std::make_unique<FhirImpl<ProcessT>>();
    for (const auto profileType : magic_enum::enum_values<model::ProfileType>())
    {
        Expect3(ConfigurationBase::ERP::requiredProfiles.contains(profileType) ||
                    ConfigurationBase::MedicationExporter::requiredProfiles.contains(profileType),
                "Profile type must be required by at least one ProcessType: " +
                    std::string{magic_enum::enum_name(profileType)},
                std::logic_error);
    }
    if (init == Init::now)
    {
        mInstance->ensureInitialized();
    }
}

Fhir::~Fhir() = default;

template void Fhir::init<ConfigurationBase::ERP>(Init init);
template void Fhir::init<ConfigurationBase::MedicationExporter>(Init init);
