/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/TestUtils.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/model/Timestamp.hxx"
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
    const auto& config = Configuration::instance();
    using BundleFactory = typename model::ResourceFactory<BundleT>;
    typename BundleFactory::Options opt;
    switch (config.prescriptionDigestRefType())
    {
        case Configuration::PrescriptionDigestRefType::relative:
            opt.validatorOptions.template emplace<fhirtools::ValidatorOptions>({
                .levels =
                    {
                        .unreferencedBundledResource = fhirtools::Severity::warning,
                        .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning,
                    },
            });
            break;
        case Configuration::PrescriptionDigestRefType::uuid:
            break;
    }

    return BundleFactory::fromXml(xmlDoc, *StaticData::getXmlValidator(), opt).getValidated(profileType);
}


template model::Bundle getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);
template model::ErxReceipt getValidatedErxReceiptBundle(std::string_view xmlDoc, model::ProfileType profileType);

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const fhirtools::Date& startDate)
{
    date::local_days epoch{date::days::zero()};
    const auto& config = Configuration::instance();
    const auto& allViews = config.fhirResourceViewConfiguration().allViews();
    auto refView = std::ranges::find(allViews, viewId, &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
    Expect3(refView != allViews.end(), "no such view: " + viewId, std::logic_error);
    date::year_month_day refStart{(*refView)->mStart.value_or(epoch)};
    auto shift = date::sys_days{date::year_month_day{startDate}} - date::sys_days{refStart};
    for (const auto& viewInfo: allViews)
    {
        if (viewInfo->mStart)
        {
            date::year_month_day viewStart{*(viewInfo)->mStart};
            auto shiftedStart = date::year_month_day{date::sys_days{viewStart} + shift};
            envGuards.emplace_back(viewInfo->mEnvName + "_VALID_FROM", date::format("%Y-%m-%d", shiftedStart));
        }
        if (viewInfo->mEnd)
        {
            date::year_month_day viewEnd{*viewInfo->mEnd};
            auto shiftedEnd = date::year_month_day{date::sys_days{viewEnd} + shift};
            envGuards.emplace_back(viewInfo->mEnvName + "_VALID_UNTIL", date::format("%Y-%m-%d", shiftedEnd));
        }
    }
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const date::sys_days& startDate)
    : ShiftFhirResourceViewsGuard{viewId, fhirtools::Date{date::year_month_day{startDate}, fhirtools::Date::Precision::day}}
{

}

} // namespace testutils
