/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/TestUtils.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/Timestamp.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "test/util/StaticData.hxx"

#include <date/tz.h>
#include <algorithm>
#include <chrono>
#include <regex>

namespace testutils
{

std::set<fhirtools::ValidationError> validationResultFilter(const fhirtools::ValidationResults& validationResult,
                                                            const fhirtools::ValidatorOptions& options)
{
    fhirtools::ValidationResults allowList;
    // TODO ERP-12940 begin: remove allowlist entries:
    allowList.add(options.levels.unreferencedBundledResource,
                  "reference must be resolvable: Binary/PrescriptionDigest-.+",
                  "Bundle.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[1].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[3].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[1].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[3].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    // TODO ERP-12940 end

    // allows description strings to partly match to be considered equal.
    auto customLess = [](const fhirtools::ValidationError& l, const fhirtools::ValidationError& r) {
        if (l.profile == r.profile && l.fieldName == r.fieldName && l.reason < r.reason)
        {
            if (l.reason.index() == r.reason.index() &&
                std::holds_alternative<fhirtools::ValidationError::MessageReason>(l.reason))
            {
                auto lr = std::get<fhirtools::ValidationError::MessageReason>(l.reason);
                auto rr = std::get<fhirtools::ValidationError::MessageReason>(r.reason);
                if (std::get<fhirtools::Severity>(lr) == std::get<fhirtools::Severity>(rr))
                {
                    auto lrr = std::get<std::string>(lr);
                    auto rrr = std::get<std::string>(rr);
                    // consider lr starts_with rr and vice versa as equals.
                    std::regex reg1(lrr);
                    std::regex reg2(rrr);
                    if (std::regex_match(rrr, reg1) || std::regex_match(lrr, reg2))
                    {
                        return false;
                    }
                }
            }
        }
        return l < r;
    };

    std::set<fhirtools::ValidationError> filteredValidationErrors;
    std::ranges::set_difference(validationResult.results(), allowList.results(),
                                std::inserter(filteredValidationErrors, filteredValidationErrors.begin()),
                                customLess);
    return filteredValidationErrors;
}

template<typename BundleT>
BundleT getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType)
{
    // TODO: validate ERP-23976
    (void)profileType;
    return BundleT::fromXmlNoValidation(xmlDoc);
    // const auto& config = Configuration::instance();
    // using BundleFactory = typename model::ResourceFactory<BundleT>;
    // typename BundleFactory::Options opt;
    // switch (config.prescriptionDigestRefType())
    // {
    //     case Configuration::PrescriptionDigestRefType::relative:
    //         opt.validatorOptions.template emplace<fhirtools::ValidatorOptions>({
    //             .levels =
    //                 {
    //                     .unreferencedBundledResource = fhirtools::Severity::warning,
    //                     .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning,
    //                 },
    //         });
    //         break;
    //     case Configuration::PrescriptionDigestRefType::uuid:
    //         break;
    // }
    //
    // return BundleFactory::fromXml(xmlDoc, *StaticData::getXmlValidator(), opt).getValidated(profileType);
}

void bestEffortValidate(const model::UnspecifiedResource& res)
{
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const auto& resourceType = res.getResourceType();
    fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true};
    std::shared_ptr<fhirtools::FhirStructureRepository> repoView;
    //NOLINTBEGIN(bugprone-branch-clone)
    // TODO: also validate Bundle ERP-23976
    if (resourceType == model::OperationOutcome::resourceTypeName)
    {
        repoView = fhirInstance.structureRepository(model::Timestamp::now()).latest().view(backend);
    }
    else if (auto profileName = res.getProfileName())
    {
        auto viewList = fhirInstance.structureRepository(model::Timestamp::now());
        fhirtools::DefinitionKey key{*profileName};
        ASSERT_TRUE(key.version.has_value()) << " missing version in profile: " << *profileName;
        repoView = viewList.match(backend, key.url, *key.version);
        ASSERT_NE(repoView, nullptr) << " no view for profile: " << *profileName;
    }
    else if (resourceType != model::Bundle::resourceTypeName)
    {
        repoView = fhirInstance.structureRepository(model::Timestamp::now()).latest().view(backend);
    }
    //NOLINTEND(bugprone-branch-clone)
    if (repoView)
    {
        auto validationResult = res.genericValidate(model::ProfileType::fhir, options, repoView);
        auto filteredValidationErrors = validationResultFilter(validationResult, options);
        for (const auto& item : filteredValidationErrors)
        {
            ASSERT_LT(item.severity(), fhirtools::Severity::error) << to_string(item);
        }
    }
}


template model::Bundle getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);
template model::ErxReceipt getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);

std::string shiftDate(const std::string& realDate)
{
    using namespace date::literals;
    static const char format[] = "%Y-%m-%d";
    const date::days globalOffset{Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS)};

    std::istringstream realDateStream{realDate};
    date::sys_days realDateAsDate;
    date::from_stream(realDateStream, format, realDateAsDate);

    return date::format(format, realDateAsDate + globalOffset);
}

testutils::ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const AsConfiguredTag&)
{
    const auto& dirtyViews = Fhir::instance().fhirResourceViewConfiguration().allViews();
    for (const auto& view: dirtyViews)
    {
        envGuards.emplace_back(view->mEnvName + "_VALID_FROM", std::nullopt);
        envGuards.emplace_back(view->mEnvName + "_VALID_UNTIL", std::nullopt);
    }
    envGuards.emplace_back("ERP_FHIR_REFERENCE_TIME_OFFSET_DAYS", std::nullopt);
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const fhirtools::Date& startDate)
    : ShiftFhirResourceViewsGuard(calculateOffset(viewId, startDate))
{
}

std::chrono::days ShiftFhirResourceViewsGuard::calculateOffset(const std::string& viewId, const fhirtools::Date& startDate)
{
    std::list<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceViewConfiguration::ViewConfig>>> cleanViews;
    {
        // clear any Environment-Variables affecting validity so we get a cleanViews that has mStart and mEnd as configured
        ShiftFhirResourceViewsGuard clear{asConfigured};
        cleanViews = Fhir::instance().fhirResourceViewConfiguration().allViews();
    }
    date::local_days epoch{date::days::zero()};
    auto refView = std::ranges::find(cleanViews, viewId, &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
    Expect3(refView != cleanViews.end(), "no such view: " + viewId, std::logic_error);
    date::year_month_day refStart{(*refView)->mStart.value_or(epoch)};
    return date::sys_days{date::year_month_day{startDate}} - date::sys_days{refStart};
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::chrono::days& offset)
{
    envGuards.emplace_back("ERP_FHIR_REFERENCE_TIME_OFFSET_DAYS", std::to_string(offset.count()));
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const date::sys_days& startDate)
    : ShiftFhirResourceViewsGuard{viewId, fhirtools::Date{date::year_month_day{startDate}, fhirtools::Date::Precision::day}}
{
}

} // namespace testutils
