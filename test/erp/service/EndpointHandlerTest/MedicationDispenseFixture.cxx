/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MedicationDispenseFixture.hxx"

#include "fhirtools/repository/FhirVersion.hxx"
#include "shared/model/ProfileType.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <regex>
#include <string>
#include <utility>

std::list<MedicationDispenseFixture::ProfileValidityTestParams> MedicationDispenseFixture::profileValidityTestParameters()
{
    using enum ExpectedResult;
    using enum BodyType;
    using V = ResourceTemplates::Versions;

    return {
        { success , "2025-04-15", single, V::GEM_ERP_1_2 },
        { success , "2025-04-15", single, V::GEM_ERP_1_3 },
        { failure , "2025-04-15", single, V::GEM_ERP_1_4 },
        { failure , "2025-04-15", single, V::GEM_ERP_1_5_2 },
        { success , "2025-04-15", bundle, V::GEM_ERP_1_2 },
        { success , "2025-04-15", bundle, V::GEM_ERP_1_3 },
        { failure , "2025-04-15", bundle, V::GEM_ERP_1_4 },
        { failure , "2025-04-15", bundle, V::GEM_ERP_1_5_2 },
        { success , "2025-04-15", bundleNoProfile, V::GEM_ERP_1_2 },
        { success , "2025-04-15", bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , "2025-04-15", bundleNoProfile, V::GEM_ERP_1_4 },
        { failure , "2025-04-15", bundleNoProfile, V::GEM_ERP_1_5_2 },
        { failure , "2025-04-15", parameters, V::GEM_ERP_1_2 },
        { failure , "2025-04-15", parameters, V::GEM_ERP_1_3 },
        { success , "2025-04-15", parameters, V::GEM_ERP_1_4 },
        { failure , "2025-04-15", parameters, V::GEM_ERP_1_5_2 },

        { failure , "2025-04-16", single, V::GEM_ERP_1_2 },
        { failure , "2025-04-16", single, V::GEM_ERP_1_3 },
        { failure , "2025-04-16", single, V::GEM_ERP_1_4 },
        { failure , "2025-04-16", single, V::GEM_ERP_1_5_2 },
        { failure , "2025-04-16", bundle, V::GEM_ERP_1_2 },
        { failure , "2025-04-16", bundle, V::GEM_ERP_1_3 },
        { failure , "2025-04-16", bundle, V::GEM_ERP_1_4 },
        { failure , "2025-04-16", bundle, V::GEM_ERP_1_5_2 },
        { failure , "2025-04-16", bundleNoProfile, V::GEM_ERP_1_2 },
        { failure , "2025-04-16", bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , "2025-04-16", bundleNoProfile, V::GEM_ERP_1_4 },
        { failure , "2025-04-16", bundleNoProfile, V::GEM_ERP_1_5_2 },
        { failure , "2025-04-16", parameters, V::GEM_ERP_1_2 },
        { failure , "2025-04-16", parameters, V::GEM_ERP_1_3 },
        { success , "2025-04-16", parameters, V::GEM_ERP_1_4 },
        { failure , "2025-04-16", parameters, V::GEM_ERP_1_5_2 },

        { failure , "2025-09-30", single, V::GEM_ERP_1_2 },
        { failure , "2025-09-30", single, V::GEM_ERP_1_3 },
        { failure , "2025-09-30", single, V::GEM_ERP_1_4 },
        { failure , "2025-09-30", single, V::GEM_ERP_1_5_2 },
        { failure , "2025-09-30", bundle, V::GEM_ERP_1_2 },
        { failure , "2025-09-30", bundle, V::GEM_ERP_1_3 },
        { failure , "2025-09-30", bundle, V::GEM_ERP_1_4 },
        { failure , "2025-09-30", bundle, V::GEM_ERP_1_5_2 },
        { failure , "2025-09-30", bundleNoProfile, V::GEM_ERP_1_2 },
        { failure , "2025-09-30", bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , "2025-09-30", bundleNoProfile, V::GEM_ERP_1_4 },
        { failure , "2025-09-30", bundleNoProfile, V::GEM_ERP_1_5_2 },
        { failure , "2025-09-30", parameters, V::GEM_ERP_1_2 },
        { failure , "2025-09-30", parameters, V::GEM_ERP_1_3 },
        { success , "2025-09-30", parameters, V::GEM_ERP_1_4 },
        { failure , "2025-09-30", parameters, V::GEM_ERP_1_5_2 },

        { failure , "2025-10-01", single, V::GEM_ERP_1_2 },
        { failure , "2025-10-01", single, V::GEM_ERP_1_3 },
        { failure , "2025-10-01", single, V::GEM_ERP_1_4 },
        { failure , "2025-10-01", single, V::GEM_ERP_1_5_2 },
        { failure , "2025-10-01", bundle, V::GEM_ERP_1_2 },
        { failure , "2025-10-01", bundle, V::GEM_ERP_1_3 },
        { failure , "2025-10-01", bundle, V::GEM_ERP_1_4 },
        { failure , "2025-10-01", bundle, V::GEM_ERP_1_5_2 },
        { failure , "2025-10-01", bundleNoProfile, V::GEM_ERP_1_2 },
        { failure , "2025-10-01", bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , "2025-10-01", bundleNoProfile, V::GEM_ERP_1_4 },
        { failure , "2025-10-01", bundleNoProfile, V::GEM_ERP_1_5_2 },
        { failure , "2025-10-01", parameters, V::GEM_ERP_1_2 },
        { failure , "2025-10-01", parameters, V::GEM_ERP_1_3 },
        { success , "2025-10-01", parameters, V::GEM_ERP_1_4 },
        { success , "2025-10-01", parameters, V::GEM_ERP_1_5_2 },
    };
}

std::list<MedicationDispenseFixture::MaxWhenHandedOverTestParams> MedicationDispenseFixture::maxWhenHandedOverTestParameters()
{
    using enum ExpectedResult;
    using enum BodyType;
    using V = ResourceTemplates::Versions;

    return {
        { success , {"2025-02-15", "2025-03-15", "2025-04-15"}, bundle, V::GEM_ERP_1_3 },
        { failure , {"2025-04-16", "2025-03-15", "2025-04-15"}, bundle, V::GEM_ERP_1_3 },
        { failure , {"2025-02-15", "2025-04-16", "2025-04-15"}, bundle, V::GEM_ERP_1_3 },
        { failure , {"2025-02-15", "2025-03-15", "2025-04-16"}, bundle, V::GEM_ERP_1_3 },
        { failure , {"2025-10-01", "2025-03-15", "2025-04-16"}, bundle, V::GEM_ERP_1_3 },

        { success , {"2025-02-15", "2025-03-15", "2025-04-15"}, bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , {"2025-04-16", "2025-03-15", "2025-04-15"}, bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , {"2025-02-15", "2025-04-16", "2025-04-15"}, bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , {"2025-02-15", "2025-03-15", "2025-04-16"}, bundleNoProfile, V::GEM_ERP_1_3 },
        { failure , {"2025-10-01", "2025-03-15", "2025-04-16"}, bundleNoProfile, V::GEM_ERP_1_3 },

        { failure , {"2025-01-12", "2025-01-11", "2025-01-14"}, parameters, V::GEM_ERP_1_4 },
        { success , {"2025-01-15", "2025-01-11", "2025-01-14"}, parameters, V::GEM_ERP_1_4 },
        { success , {"2025-01-12", "2025-01-15", "2025-01-14"}, parameters, V::GEM_ERP_1_4 },
        { success , {"2025-01-12", "2025-01-11", "2025-01-15"}, parameters, V::GEM_ERP_1_4 },
        { success , {"2025-10-01", "2025-01-11", "2025-01-15"}, parameters, V::GEM_ERP_1_4 },

        { failure , {"2025-01-12", "2025-01-11", "2025-01-14"}, parameters, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-15", "2025-01-11", "2025-01-14"}, parameters, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-12", "2025-01-15", "2025-01-14"}, parameters, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-12", "2025-01-11", "2025-01-15"}, parameters, V::GEM_ERP_1_5_2 },
        { success , {"2025-10-01", "2025-01-11", "2025-01-15"}, parameters, V::GEM_ERP_1_5_2 },
    };
}

std::string MedicationDispenseFixture::medicationDispenseBody(const BodyOptions& opt) const
{
    switch (opt.bodyType)
    {
        case BodyType::single:
            return medicationDispenseXml(opt);
        case BodyType::bundle:
            return medicationDispenseBundleXml(opt);
        case BodyType::bundleNoProfile:
            return medicationDispenseBundleXmlNoProfile(opt);
        case BodyType::parameters:
            switch (opt.endpointType)
            {
                case EndpointType::close:
                    return operationInputParamers(model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input, opt);
                case EndpointType::dispense:
                    return operationInputParamers(model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input, opt);
            }
            Fail("Unknown value for BodyType: " + std::to_string(static_cast<uintmax_t>(opt.bodyType)));
    }
    Fail("Unknown value for BodyType: " + std::to_string(static_cast<uintmax_t>(opt.bodyType)));
}

std::string MedicationDispenseFixture::medicationDispenseXml(BodyOptions opt) const
{
    if (!opt.overrideVersion)
    {
        opt.overrideVersion = std::min(ResourceTemplates::Versions::GEM_ERP_1_3, ResourceTemplates::Versions::GEM_ERP_current());
    }
    auto options = dispenseOptions;
    options.gematikVersion =
        opt.overrideVersion.value_or(std::min(ResourceTemplates::Versions::GEM_ERP_1_3, dispenseOptions.gematikVersion));
    if (!opt.overrideWhenHandedOver.empty())
    {
        Expect(opt.overrideWhenHandedOver.size() == 1, "Extra overrideWhenHandedOver values.");
        options.whenHandedOver = opt.overrideWhenHandedOver.front();
    }
    else
    {
        options.whenHandedOver = testutils::shiftDate(lastValidDayOf_1_3);
    }
    return ResourceTemplates::medicationDispenseXml(options);
}

namespace {
struct GemVer {
    ResourceTemplates::Versions::GEM_ERP operator()(const model::Timestamp& ts)
    {
        return ResourceTemplates::Versions::GEM_ERP_current(ts);
    }
    ResourceTemplates::Versions::GEM_ERP operator()(const std::string& ts)
    {
        try {
            return ResourceTemplates::Versions::GEM_ERP_current(model::Timestamp::fromGermanDate(ts));
        }
        catch (const model::ModelException& ex)
        {
            return ResourceTemplates::Versions::GEM_ERP_current();
        }
    }
};
}

std::list<ResourceTemplates::MedicationDispenseOptions>
MedicationDispenseFixture::getMultiMedicationDispenseOptions(const BodyOptions& opt) const
{
    std::list<ResourceTemplates::MedicationDispenseOptions> result;
    auto overrideWhenHandedOverExtra = opt.overrideWhenHandedOver.empty() ? testutils::shiftDate(lastValidDayOf_1_3)
    : opt.overrideWhenHandedOver.back();
    auto overrideWhenHandedOver = opt.overrideWhenHandedOver.begin();

    for (size_t idx = 0; idx < std::max(opt.overrideWhenHandedOver.size(), 2ul); ++idx)
    {
        auto& single = result.emplace_back<ResourceTemplates::MedicationDispenseOptions>({
            .id{model::MedicationDispenseId{prescriptionId, 1}},
            .telematikId = telematikId,
        });
        if (overrideWhenHandedOver != opt.overrideWhenHandedOver.end())
        {
            single.whenHandedOver = *overrideWhenHandedOver;
            ++overrideWhenHandedOver;
        }
        else
        {
            single.whenHandedOver = overrideWhenHandedOverExtra;
        }
        if (opt.overrideVersion)
        {
            single.gematikVersion = *opt.overrideVersion;
        }
        else
        {
            single.gematikVersion = std::visit(GemVer{}, single.whenHandedOver);
        }
        if (single.gematikVersion >= ResourceTemplates::Versions::GEM_ERP_1_4)
        {
            auto& medicationOptions = single.medication.emplace<ResourceTemplates::MedicationOptions>();
            medicationOptions.version = single.gematikVersion;
        }
    }
    return result;
}


std::string MedicationDispenseFixture::medicationDispenseBundleXmlNoProfile(const BodyOptions& opt) const
{
    /* ERP-22297: remove meta/profile */
    const std::string profileUrl{value(profile(model::ProfileType::MedicationDispenseBundle))};
    std::string result =
        regex_replace(medicationDispenseBundleXml(opt),
                      std::regex{"<meta><profile value=\"" + profileUrl + "\\|" +
                                 to_string(ResourceTemplates::Versions::GEM_ERP_current()) + "\"/></meta>"},
                      "");
    EXPECT_EQ(result.find(R"("https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_CloseOperationInputBundle")"),
              std::string::npos);
    return result;
}

std::string MedicationDispenseFixture::operationInputParamers(model::ProfileType profileType, const BodyOptions& opt) const
{
    if (!opt.overrideVersion && ResourceTemplates::Versions::GEM_ERP_current() < ResourceTemplates::Versions::GEM_ERP_1_4)
    {
        return notSupported;
    }
    auto multiOptions = getMultiMedicationDispenseOptions(opt);
    const auto versionFromWhenHandedOver =
        std::views::transform([](const ResourceTemplates::MedicationDispenseOptions& md) {
            return std::visit(GemVer{}, md.whenHandedOver);
        });
    auto gemVer = std::ranges::max(multiOptions | versionFromWhenHandedOver);
    return ResourceTemplates::medicationDispenseOperationParametersXml({
        .profileType = profileType,
        .version = opt.overrideVersion.value_or(std::max(gemVer, ResourceTemplates::Versions::GEM_ERP_1_4)),
        .medicationDispenses = std::move(multiOptions),
    });
}

std::string MedicationDispenseFixture::medicationDispenseBundleXml(BodyOptions opt) const
{
    if (!opt.overrideVersion)
    {
        opt.overrideVersion = std::min(ResourceTemplates::Versions::GEM_ERP_1_3, ResourceTemplates::Versions::GEM_ERP_current());
    }
    auto multiOptions = getMultiMedicationDispenseOptions(opt);
    return ResourceTemplates::medicationDispenseBundleXml({
        .gematikVersion = *opt.overrideVersion,
        .medicationDispenses = multiOptions,
    });
}

std::ostream& operator << (std::ostream& out, const MedicationDispenseFixture::ProfileValidityTestParams& params)
{
    out << "{ ";
    out << R"("result": ")" << magic_enum::enum_name(params.result) << R"(", )";
    out << R"("date": ")" << params.date << R"(", )";
    out << R"("bodyType": ")" << magic_enum::enum_name(params.bodyType) << R"(", )";
    out << R"("version": ")" << to_string(params.version) << R"(")";
    out << " }";
    return out;
}

std::ostream& operator<<(std::ostream& out, const MedicationDispenseFixture::MaxWhenHandedOverTestParams& params)
{
    out << "{ ";
    out << R"("result": ")" << magic_enum::enum_name(params.result) << R"(", )";
    out << R"("whenHandedOver": [")" << String::join(params.whenHandedOver, R"(", ")") << R"(" ], )";
    out << R"("bodyType": ")" << magic_enum::enum_name(params.bodyType) << R"(", )";
    out << R"("version": ")" << to_string(params.version) << R"(")";
    out << " }";
    return out;
}
