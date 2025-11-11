/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Fhir.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/VersionMapper.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/views/CodeCachingView.hxx"
#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/repository/views/FhirResourceViewVerifier.hxx"
#include "fhirtools/repository/views/VersionMappingView.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Configuration.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"

#include <algorithm>
#include <map>
#include <shared_mutex>
#include <string>

std::unique_ptr<Fhir> Fhir::mInstance;

namespace
{

class FhirImplBase : public Fhir
{
public:
    using ViewPtr = fhirtools::FhirResourceViewList::ViewPtr;
    using ViewConfig = fhirtools::FhirResourceViewConfiguration::ViewConfig;

    ViewPtr acquireView(const ViewConfig& viewConfig) const;
    virtual ViewPtr wrapWithVersionMapper(ViewPtr view) const = 0;

    const fhirtools::FhirStructureRepositoryBackend& backend() const override
    {
        return mBackend;
    }

    fhirtools::FhirStructureRepositoryBackend mBackend;
    mutable std::shared_mutex mViewsByConfigIdMutex;
    mutable std::map<std::string, ViewPtr> mViewsByConfigId;
};

FhirImplBase::ViewPtr FhirImplBase::acquireView(const ViewConfig& viewConfig) const
{
    std::shared_lock read{mViewsByConfigIdMutex};
    auto view = mViewsByConfigId.find(viewConfig.mId);
    if (view != mViewsByConfigId.end())
    {
        return view->second;
    }
    read.unlock();

    std::unique_lock write{mViewsByConfigIdMutex};
    view = mViewsByConfigId.lower_bound(viewConfig.mId);
    if (view != mViewsByConfigId.end() && view->first == viewConfig.mId)
    {
        return view->second;
    }
    std::string newId = viewConfig.mId + "-cached";
    auto newView =
        fhirtools::CodeCachingView::create(std::move(newId), wrapWithVersionMapper(viewConfig.view(&mBackend)));
    return mViewsByConfigId.emplace_hint(view, viewConfig.mId, std::move(newView))->second;
}

template<config::ProcessType ProcessT>
class FhirImpl final : public FhirImplBase
{
public:

    FhirImpl();
    ~FhirImpl() override = default;

    const FhirConverter& converter() const override
    {
        return mConverter;
    }

    fhirtools::FhirResourceViewList allViews() const override;

    fhirtools::FhirResourceViewList structureRepository(const model::Timestamp& referenceTimestamp) const override;

    fhirtools::FhirResourceViewConfiguration fhirResourceViewConfiguration() const override;

    fhirtools::ValidatorOptions defaultValidatorOptions(model::ProfileType profileType,
                                                        const model::Timestamp& referenceTimestamp) const override;

    ViewPtr wrapWithVersionMapper(ViewPtr unmapped) const override;

    void ensureInitialized() override;

    void load();
    void validateViews() const;
    void validateProfileRequirements(const fhirtools::FhirResourceViewList& viewList,
                                     const model::Timestamp& timestamp) const;

    FhirConverter mConverter;
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
{
}
template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::load()
{
    const auto& config = Configuration::instance();
    auto filesAsString = config.getArray(ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS);
    std::list<std::filesystem::path> files;
    std::transform(filesAsString.begin(), filesAsString.end(), std::back_inserter(files), [](const auto& str) {
        return std::filesystem::path{str};
    });
    TLOG(INFO) << "Loading FHIR structure repository.";
    auto resolver = config.fhirResourceGroupConfiguration<ProcessT>();
    for (const auto& [url, version] : config.synthesizeCodesystem())
    {
        mBackend.synthesizeCodeSystem(url, version, resolver);
    }
    for (const auto& [url, version] : config.synthesizeValuesets())
    {
        mBackend.synthesizeValueSet(url, version, resolver);
    }
    mBackend.load(files, resolver);
}

template<config::ProcessType ProcessT>
fhirtools::FhirResourceViewList FhirImpl<ProcessT>::allViews() const
{
    const auto& config = Configuration::instance();
    auto viewConfig = config.fhirResourceViewConfiguration<ProcessT>();
    return fhirtools::FhirResourceViewList{std::bind_front(&FhirImplBase::acquireView, this), viewConfig.allViews()};
}

template<config::ProcessType ProcessT>
fhirtools::FhirResourceViewList
FhirImpl<ProcessT>::structureRepository(const model::Timestamp& referenceTimestamp) const
{
    const auto& config = Configuration::instance();
    auto viewConfig = config.fhirResourceViewConfiguration<ProcessT>();
    fhirtools::FhirResourceViewConfiguration::ViewList viewList;
    const auto& referenceDate = fhirtools::Date{referenceTimestamp.toGermanDate()};
    viewList = viewConfig.getViewInfo(referenceDate);
    Expect(! viewList.empty(), "invalid reference date: " + referenceDate.toString(false));
    return fhirtools::FhirResourceViewList{std::bind_front(&FhirImplBase::acquireView, this), viewList};
}


template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::validateViews() const
{
    const auto& config = Configuration::instance();
    auto viewConfig = config.fhirResourceViewConfiguration<ProcessT>();
    std::set<date::local_days> viewDates;
    for (const auto& viewConf : viewConfig.allViews())
    {
        auto view = fhirtools::CodeCachingView::create(viewConf->mId + "-cached", viewConf->view(&mBackend));
        fhirtools::FhirResourceViewVerifier verifier{mBackend, view.get()};
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

std::optional<model::Timestamp> fromGermanDateWithOffset(const std::string& date, date::days offset)
{
    if (date.empty())
    {
        return std::nullopt;
    }
    date::local_days day;
    std::istringstream strm{date};
    date::from_stream(strm, "%Y-%m-%d", day);
    return model::Timestamp{model::Timestamp::GermanTimezone, day + offset};
}


template<config::ProcessType ProcessT>
void FhirImpl<ProcessT>::validateProfileRequirements(const fhirtools::FhirResourceViewList& viewList,
                                                     const model::Timestamp& timestamp) const
{
    const date::days globalOffset{
        Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS)};
    for (const auto& profileTypeReq : ProcessT::requiredProfiles)
    {
        auto from = fromGermanDateWithOffset(profileTypeReq.second.from, globalOffset);
        auto until = fromGermanDateWithOffset(profileTypeReq.second.until, globalOffset);
        const bool shouldExist = ((!until || timestamp <= *until) && (!from || timestamp >= *from));
        if (profileTypeReq.first != model::ProfileType::fhir)
        {
            std::optional<fhirtools::DefinitionKey> key;
            for (const auto& view : viewList.all())
            {
                key = profileWithVersion(profileTypeReq.first, *view);
                if (key)
                {
                    TVLOG(2) << view->id() << " found profile: " << *key;
                    break;
                }
                TVLOG(3) << view->id() << " not found profile: "
                         << profile(profileTypeReq.first).value_or(magic_enum::enum_name(profileTypeReq.first));
            }
            Expect3(key.has_value() || !shouldExist,
                    "could not resolve " + std::string{magic_enum::enum_name(profileTypeReq.first)} +
                        " for date: " + timestamp.toGermanDate(),
                    std::logic_error);
            Expect3(!key.has_value() || shouldExist,
                    std::string{magic_enum::enum_name(profileTypeReq.first)} +
                    " should not be resolveable for date: " + timestamp.toGermanDate(),
                    std::logic_error);
        }
    }
}


template<config::ProcessType ProcessT>
fhirtools::FhirResourceViewConfiguration FhirImpl<ProcessT>::fhirResourceViewConfiguration() const
{
    return Configuration::instance().fhirResourceViewConfiguration<ProcessT>();
}

template<config::ProcessType ProcessT>
fhirtools::ValidatorOptions
FhirImpl<ProcessT>::defaultValidatorOptions(model::ProfileType profileType,
                                            const model::Timestamp& referenceTimestamp) const
{
    using enum ConfigurationKey;
    using Severity = fhirtools::Severity;
    const auto& config = Configuration::instance();
    fhirtools::ValidatorOptions options;
    options.levels.mandatoryResolvableReferenceFailure =
        config.get<Severity>(ProcessT::FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE);
    options.levels.unreferencedBundledResource =
        config.get<Severity>(ProcessT::FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE);
    options.levels.unreferencedContainedResource =
        config.get<Severity>(ProcessT::FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE);
    options.levels.missingOrExtraMetaProfile =
        config.get<Severity>(ProcessT::FHIR_VALIDATION_LEVELS_MISSING_OR_EXTRA_META_PROFILE);

    if constexpr (std::is_same_v<Configuration::ERP, ProcessT>)
    {
        const date::days globalOffset{
            Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS)};
        const auto urnUuidCheckFrom = model::Timestamp::fromGermanDate(
            config.getStringValue(ConfigurationKey::FHIR_VALIDATION_URN_UUID_CHECK_FROM));
        if (referenceTimestamp >= (urnUuidCheckFrom + globalOffset))
        {
            options.levels.invalidUrnUuidInUri.emplace(Severity::error);
        }
        const auto fullUrlCheckFrom = model::Timestamp::fromGermanDate(
            config.getStringValue(ConfigurationKey::FHIR_VALIDATION_FULL_URL_AND_BUNDLE_REFERENCE_CHECK_FROM));
        if (referenceTimestamp >= (fullUrlCheckFrom + globalOffset))
        {
            A_26229_01.start("activate id check.");
            options.levels.bundleFullUrlIdMissmatch.emplace(Severity::error);
            A_26229_01.finish();
            A_26233_01.start("activate fullUrl format check.");
            options.levels.bundleFullUrlInvalidFormat.emplace(Severity::error);
            A_26233_01.finish();
            A_27648.start("activate missing id in bundled resource check");
            options.levels.bundledResourceMissingId.emplace(Severity::error);
            A_27648.finish();
            A_27649.start("activate unresolveable references in bundle check");
            options.levels.unresolveableReferenceInBundle.emplace(Severity::error);
            A_27649.finish();
        }
        const auto rejectUnslicedExtensionsFrom =
            model::Timestamp::fromGermanDate(config.getStringValue(FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM));
        const bool rejectUnslicedExtensions = referenceTimestamp >= (rejectUnslicedExtensionsFrom + globalOffset);
        if (rejectUnslicedExtensions)
        {
            options.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::closeSlicing;
        }
        if (profileType == model::ProfileType::KBV_PR_ERP_Bundle ||
            profileType == model::ProfileType::KBV_PR_EVDGA_Bundle)
        {
            options.allowNonLiteralAuthorReference =
                config.kbvValidationNonLiteralAuthorRef() == Configuration::NonLiteralAuthorRefMode::allow;
            if (! rejectUnslicedExtensions)
            {
                switch (config.kbvValidationOnUnknownExtension())
                {
                    using enum Configuration::OnUnknownExtension;
                    using ReportUnknownExtensionsMode = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode;
                    case ignore:
                        options.reportUnknownExtensions = ReportUnknownExtensionsMode::disable;
                        break;
                    case reject:
                        // unknown extensions in closed slicing will be reported as error
                        options.reportUnknownExtensions = ReportUnknownExtensionsMode::closeSlicing;
                        break;
                }
            }
            return options;
        }
    }
    return options;
}

template<config::ProcessType ProcessT>
FhirImplBase::ViewPtr FhirImpl<ProcessT>::wrapWithVersionMapper(ViewPtr unmapped) const
{
    std::string id{unmapped->id()};
    id += "-mapped";
    fhirtools::VersionMapper mapper{Configuration::instance().fhirVersionMapping<ProcessT>()};
    return fhirtools::VersionMappingView::create(std::move(id), std::move(mapper), std::move(unmapped));
}

}

const Fhir& Fhir::instance()
{
    Expect3(mInstance != nullptr, "Attempt to call Fhir::instance before init", std::logic_error);
    mInstance->ensureInitialized();
    return *mInstance;
}

fhirtools::ExpressionPtr Fhir::parseFhirPath(std::string_view fhirPath)
{
    return fhirtools::FhirPathParser::parse(std::addressof(instance().backend()), fhirPath);
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

std::shared_ptr<const fhirtools::FhirStructureRepositoryView> Fhir::defaultView() const
{
    return backend().defaultView();
}

Fhir::~Fhir() = default;

template void Fhir::init<ConfigurationBase::ERP>(Init init);
template void Fhir::init<ConfigurationBase::MedicationExporter>(Init init);
