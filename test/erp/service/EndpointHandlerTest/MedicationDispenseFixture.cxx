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
    using V = ResourceTemplates::Versions;

    return {
        { success , "2026-06-30", V::GEM_ERP_1_5_2 },
        { failure , "2026-06-30", V::GEM_ERP_1_6_2 },

        { success , "2026-07-01", V::GEM_ERP_1_5_2 },
        { success , "2026-07-01", V::GEM_ERP_1_6_2 },

        { failure , "2027-01-15", V::GEM_ERP_1_5_2 },
        { success , "2027-01-15", V::GEM_ERP_1_6_2 },
    };
}

std::list<MedicationDispenseFixture::MaxWhenHandedOverTestParams> MedicationDispenseFixture::maxWhenHandedOverTestParameters()
{
    using enum ExpectedResult;
    using V = ResourceTemplates::Versions;

    return {

        { failure , {"2025-01-12", "2025-01-11", "2025-01-14"}, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-15", "2025-01-11", "2025-01-14"}, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-12", "2025-01-15", "2025-01-14"}, V::GEM_ERP_1_5_2 },
        { failure , {"2025-01-12", "2025-01-11", "2025-01-15"}, V::GEM_ERP_1_5_2 },
        { success , {"2025-10-01", "2025-01-11", "2025-01-15"}, V::GEM_ERP_1_5_2 },
        { success , {"2026-01-10", "2026-01-11", "2025-01-15"}, V::GEM_ERP_1_5_2 },
        { failure , {"2026-01-10", "2027-01-15", "2025-01-15"}, V::GEM_ERP_1_5_2 },

        { failure , {"2026-01-10", "2026-06-30", "2025-01-15"}, V::GEM_ERP_1_6_2 },
        { success , {"2026-07-01", "2026-01-10", "2025-01-15"}, V::GEM_ERP_1_6_2 },
        { success , {"2026-01-10", "2026-07-01", "2025-01-15"}, V::GEM_ERP_1_6_2 },
        { success , {"2026-01-10", "2025-01-15", "2026-07-01"}, V::GEM_ERP_1_6_2 },
        { success , {"2026-01-10", "2027-01-15", "2025-01-15"}, V::GEM_ERP_1_6_2 },
    };
}

std::string MedicationDispenseFixture::medicationDispenseBody(const BodyOptions& opt) const
{
    switch (opt.endpointType)
    {
        case EndpointType::close:
            return operationInputParamers(model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input, opt);
        case EndpointType::dispense:
            return operationInputParamers(model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input, opt);
    }
    Fail("Unknown value for EndpointType: " + std::to_string(static_cast<uintmax_t>(opt.endpointType)));
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
    auto fallbackDate = model::Timestamp::now().toGermanDate();
    auto overrideWhenHandedOverExtra =
        opt.overrideWhenHandedOver.empty() ? fallbackDate : opt.overrideWhenHandedOver.back();
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
        auto& medicationOptions = single.medication.emplace<ResourceTemplates::MedicationOptions>();
        medicationOptions.version = single.gematikVersion;
    }
    return result;
}

std::string MedicationDispenseFixture::operationInputParamers(model::ProfileType profileType, const BodyOptions& opt) const
{
    auto multiOptions = getMultiMedicationDispenseOptions(opt);
    const auto versionFromWhenHandedOver =
        std::views::transform([](const ResourceTemplates::MedicationDispenseOptions& md) {
            return std::visit(GemVer{}, md.whenHandedOver);
        });
    auto gemVer = std::ranges::max(multiOptions | versionFromWhenHandedOver);
    return ResourceTemplates::medicationDispenseOperationParametersXml({
        .profileType = profileType,
        .version = opt.overrideVersion.value_or(gemVer),
        .medicationDispenses = std::move(multiOptions),
    });
}

std::ostream& operator << (std::ostream& out, const MedicationDispenseFixture::ProfileValidityTestParams& params)
{
    out << "{ ";
    out << R"("result": ")" << magic_enum::enum_name(params.result) << R"(", )";
    out << R"("date": ")" << params.date << R"(", )";
    out << R"("version": ")" << to_string(params.version) << R"(")";
    out << " }";
    return out;
}

std::ostream& operator<<(std::ostream& out, const MedicationDispenseFixture::MaxWhenHandedOverTestParams& params)
{
    out << "{ ";
    out << R"("result": ")" << magic_enum::enum_name(params.result) << R"(", )";
    out << R"("whenHandedOver": [")" << String::join(params.whenHandedOver, R"(", ")") << R"(" ], )";
    out << R"("version": ")" << to_string(params.version) << R"(")";
    out << " }";
    return out;
}
