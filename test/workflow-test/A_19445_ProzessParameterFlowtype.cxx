/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpRequirements.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <string_view>
#include <date/date.h>

namespace {
struct Parameters {
    std::string_view testName;
    std::string_view signingTime;
    std::string_view acceptDate;
    std::string_view expiryDate;
    std::string_view acceptDatePkv;
};
using ParameterTuple = std::tuple<Parameters, model::PrescriptionType>;

std::ostream& operator << (std::ostream& out, const Parameters& params)
{
    out << params.testName;
    return out;
}
}


using ProzessParameterFlowtype = ErpWorkflowTestTemplate<::testing::TestWithParam<ParameterTuple>>;

TEST_P(ProzessParameterFlowtype, samples)//NOLINT(readability-function-cognitive-complexity)
{
    using fn = std::optional<std::string_view> (*)(const rapidjson::Value& object, const rapidjson::Pointer& key);
    static constexpr auto getOptionalStringValue =
        static_cast<fn>(&model::NumberAsStringParserDocument::getOptionalStringValue);
    A_19445_08.test("Prozessparameter - Flowtype");
    using namespace std::string_view_literals;
    using namespace model::resource;
    const auto& [params, prescriptionType] = GetParam();
    auto signingTime = model::Timestamp::fromXsDate(std::string{params.signingTime});
    auto acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDate});
    auto expiryDate = model::Timestamp::fromGermanDate(std::string{params.expiryDate});

    const auto kbvVersion = model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>();

    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDatePkv});
            break;
    }

    // Create Task for Workflow
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(prescriptionType));
    ASSERT_TRUE(task.has_value());
    auto accessCode = std::string{task->accessCode()};

    bool deprecatedKbv = model::ResourceVersion::deprecatedProfile(serverGematikProfileVersion());

    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::string bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = signingTime, .kvnr = kvnr, .kbvVersion = kbvVersion});

    // Activate Task with given signing time
    ASSERT_NO_FATAL_FAILURE(
        task = taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, toCadesBesSignature(bundle, signingTime))
    );
    ASSERT_TRUE(task.has_value());
    ASSERT_EQ(task->acceptDate(), acceptDate);
    ASSERT_EQ(task->expiryDate(), expiryDate);
    auto sdPrescriptionType = deprecatedKbv ? model::resource::structure_definition::deprecated::prescriptionType
                                            : model::resource::structure_definition::prescriptionType;
    auto ext = task->getExtension(sdPrescriptionType);
    ASSERT_TRUE(ext.has_value());
    auto flowTypeDisplay = ext->valueCodingDisplay();
    ASSERT_TRUE(flowTypeDisplay.has_value());
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            ASSERT_EQ(flowTypeDisplay, "Muster 16 (Apothekenpflichtige Arzneimittel)");
            break;
        case model::PrescriptionType::direkteZuweisung:
            ASSERT_EQ(flowTypeDisplay, "Muster 16 (Direkte Zuweisung)");
            break;
        case model::PrescriptionType::direkteZuweisungPkv:
            ASSERT_EQ(flowTypeDisplay, "PKV (Direkte Zuweisung)");
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            ASSERT_EQ(flowTypeDisplay, "PKV (Apothekenpflichtige Arzneimittel)");
            break;
    }
    auto taskDoc = model::NumberAsStringParserDocument::fromJson(task->serializeToJsonString());
    static const rapidjson::Pointer performerTypeCodingPtr{
            ElementName::path(ElementName{"performerType"}, 0, elements::coding, 0)
        };
    auto* performerTypeCoding = performerTypeCodingPtr.Get(taskDoc);
    ASSERT_NE(performerTypeCoding, nullptr);
    static const rapidjson::Pointer systemPtr{ElementName::path(elements::system)};
    static const rapidjson::Pointer codePtr{ElementName::path(elements::code)};
    static const rapidjson::Pointer displayPtr{ElementName::path(elements::display)};
    auto performerTypeCodingSystem = getOptionalStringValue(*performerTypeCoding, systemPtr);
    auto performerTypeCodingCode = getOptionalStringValue(*performerTypeCoding, codePtr);
    auto performerTypeCodingDisplay = getOptionalStringValue(*performerTypeCoding, displayPtr);
    ASSERT_TRUE(performerTypeCodingSystem.has_value());
    if (deprecatedKbv)
        ASSERT_EQ(std::string{*performerTypeCodingSystem}, "urn:ietf:rfc:3986");
    else
        ASSERT_EQ(std::string{*performerTypeCodingSystem}, "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_OrganizationType");
    ASSERT_TRUE(performerTypeCodingCode.has_value());
    ASSERT_EQ(std::string{*performerTypeCodingCode}, "urn:oid:1.2.276.0.76.4.54");
    ASSERT_TRUE(performerTypeCodingDisplay.has_value());
    ASSERT_EQ(std::string{*performerTypeCodingDisplay}, "Öffentliche Apotheke");
    A_19445_08.finish();
}

std::list<Parameters> samples = {
    {"normal"  , "2021-02-10", "2021-03-10", "2021-05-10", "2021-05-10"},
    {"leapYear", "2020-02-10", "2020-03-09", "2020-05-10", "2020-05-10"},
    {"leapDay" , "2020-02-29", "2020-03-28", "2020-05-29", "2020-05-29"}
};

INSTANTIATE_TEST_SUITE_P(samples, ProzessParameterFlowtype,
    ::testing::Combine(
        ::testing::ValuesIn(samples),
        ::testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                          model::PrescriptionType::direkteZuweisung,
                          model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                          model::PrescriptionType::direkteZuweisungPkv))
);
