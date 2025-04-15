/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>

#include "shared/model/Resource.hxx"
#include "shared/fhir/Fhir.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/util/XmlMemory.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ResourceManager.hxx"

#include <filesystem>
#include <regex>

struct SampleFiles {
    std::string_view xmlSample;
    std::string_view jsonSample;
    std::optional<std::string> skipReason = {};
};

static std::ostream& operator << (std::ostream& out, const SampleFiles& sample)
{
    out << R"({ "xmlSample": ")" << sample.xmlSample;
    out << R"(", "jsonSample": ")" << sample.jsonSample;
    if (sample.skipReason)
    {
        out << R"(", "skipReason": ")" << sample.skipReason.value();
    }
    out << R"(" })";
    return out;
}

class FhirConverterTest : public ::testing::TestWithParam<SampleFiles>
{
public:
    std::string replaceData(const std::string& json)
    {
        // in erx_bundle.json the signature.data field is "FakeData" while there is real data in the xml
        // replace "FakeData" with the data from the XML as we are testing conversion only
        static const std::regex fakeData("FakeData");
        static const std::string data =
                "QXVmZ3J1bmQgZGVyIENvcm9uYS1TaXR1YXRpb24ga29ubnRlIGhpZXIga3VyemZyaXN0aWcga2VpbiBCZWlzcGllbCBpbiBkZXIgTG"
                "Fib3J1bWdlYnVuZyBkZXIgZ2VtYXRpayBlcnN0ZWxsdCB3ZWRlbi4gRGllc2VzIHdpcmQgbmFjaGdlcmVpY2h0LgoKSW5oYWx0bGlj"
                "aCB1bmQgc3RydWt0dXJlbGwgaXN0IGRpZSBTZXJ2ZXJzaWduYXR1ciBkZXIgUXVpdHR1bmcgZWluZSBFbnZlbG9waW5nIENBZEVTLV"
                "NpZ25hdHVyLCBkaWUgZGVuIHNpZ25pZXJ0ZW4gRGF0ZW5zYXR6IGFuYWxvZyB6dXIgS29ubmVrdG9yLVNpZ25hdHVyIGlubmVyaGFs"
                "YiBkZXMgQVNOMS5Db250YWluZXJzIHRyYW5zcG9ydGllcnQu";
        return regex_replace(json, fakeData, data);
    }
};

class XmlBuffer
{
public:
    ~XmlBuffer()
    {
        if (buffer) {xmlFree(buffer);}
    }

    int size = 0;
    xmlChar* buffer = nullptr;
};

inline bool operator == (const XmlBuffer& lhs, const XmlBuffer& rhs)
{
    return lhs.size == rhs.size && (memcmp(lhs.buffer, rhs.buffer, gsl::narrow<size_t>(lhs.size)) == 0);
}

inline std::ostream& operator << (std::ostream& out, const XmlBuffer& xmlBuffer)
{
    out << std::string_view{reinterpret_cast<const char*>(xmlBuffer.buffer), gsl::narrow<size_t>(xmlBuffer.size)};
    return out;
}


TEST_P(FhirConverterTest, samplesFromXMLTest)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->c_str());
    }
    using namespace std::string_literals;
    const auto& converter = Fhir::instance().converter();
    auto& resourceManager = ResourceManager::instance();
    auto sample = model::NumberAsStringParserDocument::fromJson(replaceData(
            resourceManager.getStringResource("test/fhir/conversion/"s.append(GetParam().jsonSample))));
    const auto& xml = resourceManager.getStringResource("test/fhir/conversion/"s.append(GetParam().xmlSample));
    std::optional<model::NumberAsStringParserDocument> converted;
    ASSERT_NO_THROW(converted.emplace(converter.xmlStringToJson(xml)));
    EXPECT_EQ(converted->serializeToJsonString(), sample.serializeToJsonString());
}

TEST_P(FhirConverterTest, samplesToXMLTest)
{
    if (GetParam().skipReason)
    {
        GTEST_SKIP_(GetParam().skipReason->data());
    }
    using namespace std::string_literals;
    const auto& converter = Fhir::instance().converter();
    auto& resourceManager = ResourceManager::instance();
    auto sample = model::NumberAsStringParserDocument::fromJson(replaceData(
            resourceManager.getStringResource("test/fhir/conversion/"s.append(GetParam().jsonSample))));
    const auto& xmlFileName = ResourceManager::getAbsoluteFilename("test/fhir/conversion/"s.append(GetParam().xmlSample));
    UniqueXmlDocumentPtr sampleXmlDOM{xmlReadFile(xmlFileName.string().c_str(), nullptr, XML_PARSE_NOBLANKS)};
    XmlBuffer sampleBuffer;
    xmlDocDumpMemoryEnc(sampleXmlDOM.get(), &sampleBuffer.buffer, &sampleBuffer.size, "utf-8");

    UniqueXmlDocumentPtr convertedXmlDOM;
    ASSERT_NO_THROW(convertedXmlDOM = converter.jsonToXml(sample));
    XmlBuffer convertedBuffer;
    xmlDocDumpMemoryEnc(convertedXmlDOM.get(), &convertedBuffer.buffer, &convertedBuffer.size, "utf-8");
    ASSERT_TRUE(convertedBuffer.buffer);
    EXPECT_EQ(convertedBuffer , sampleBuffer);
}




using namespace std::string_view_literals;

INSTANTIATE_TEST_SUITE_P(
    gematikExamples, FhirConverterTest,
    ::testing::Values(
        // clang-format off
    //           XML file name                        JSON file Name                         skip reason (if test shall be skipped)
    SampleFiles{"audit_event.xml"                   , "audit_event.json"},
    SampleFiles{"capability_statement_both.xml"     , "capability_statement_both.json"},
    SampleFiles{"capability_statement_supplier.xml" , "capability_statement_supplier.json"},
    SampleFiles{"chargeItem_simplifier.xml"         , "chargeItem_simplifier.json"},
    SampleFiles{"communication_dispense_req.xml"    , "communication_dispense_req.json"},
    SampleFiles{"communication_info_req.xml"        , "communication_info_req.json"},
    SampleFiles{"communication_reply.xml"           , "communication_reply.json"},
    SampleFiles{"composition.xml"                   , "composition.json"},
    SampleFiles{"coverage.xml"                      , "coverage.json"},
    SampleFiles{"device.xml"                        , "device.json"},
    SampleFiles{"erx_bundle.xml"                    , "erx_bundle.json"},
    SampleFiles{"erx_composition.xml"               , "erx_composition.json"},
    SampleFiles{"kbv_bundle.xml"                    , "kbv_bundle.json"},
    SampleFiles{"medication_compounding.xml"        , "medication_compounding.json"},
    SampleFiles{"medication_dispense.xml"           , "medication_dispense.json"},
    SampleFiles{"medication_dispense_bundle.xml"    , "medication_dispense_bundle.json"},
    SampleFiles{"medication_free_text.xml"          , "medication_free_text.json"},
    SampleFiles{"medication_ingredient.xml"         , "medication_ingredient.json"},
    SampleFiles{"medication_pzn.xml"                , "medication_pzn.json"},
    SampleFiles{"medication_request.xml"            , "medication_request.json"},
    SampleFiles{"operation_outcome.xml"             , "operation_outcome.json"},
    SampleFiles{"organization.xml"                  , "organization.json"},
    SampleFiles{"patient.xml"                       , "patient.json"},
    SampleFiles{"practitioner.xml"                  , "practitioner.json"},
    SampleFiles{"practitioner_role.xml"             , "practitioner_role.json"},
    SampleFiles{"signature.xml"                     , "signature.json", "ERP-5546: Signature is not a resource."},
    SampleFiles{"task_activate_parameters.xml"      , "task_activate_parameters.json"},
    SampleFiles{"task_create_parameters.xml"        , "task_create_parameters.json"},
    SampleFiles{"task.xml"                          , "task.json"},
    SampleFiles{"task_no_secret.xml"                , "task_no_secret.json"},
    SampleFiles{"task_empty_meta_element.xml"       , "task_empty_meta_element.json", "Not valid FHIR"},
    SampleFiles{"patient_empty_adress_array.xml"    , "patient_empty_adress_array.json"}// clang-format on
        ));

INSTANTIATE_TEST_SUITE_P(fhirSpecialCases , FhirConverterTest,
        ::testing::Values(
    // clang-format off
    //           XML file name                                        JSON file Name                         skip reason (if test shall be skipped)
    SampleFiles{"ERP-4397.xml"                                      , "ERP-4397.json"},
    SampleFiles{"ERP-5478-numeric-precision.xml"                    , "ERP-5478-numeric-precision.json"},
    SampleFiles{"ERP-5002-field-refrence-type.xml"                  , "ERP-5002-field-refrence-type.json"},
    SampleFiles{"ERP-5615-unsignedInt.xml"                          , "ERP-5615-unsignedInt.json"},
    SampleFiles{"ERP-5615-positiveInt.xml"                          , "ERP-5615-positiveInt.json"},
    SampleFiles{"ERP-4468-xhtml.xml"                                , "ERP-4468-xhtml.json"},
    SampleFiles{"ERP-8163-Ampersand-encoding.xml"                   , "ERP-8163-Ampersand-encoding.json"},
    SampleFiles{"ERP-8395-primary-without-value.xml"                , "ERP-8395-primary-without-value.json"},
    SampleFiles{"ERP-8395-primary-without-value-more-extensions.xml", "ERP-8395-primary-without-value-more-extensions.json"},
    SampleFiles{"ERP-8395-primary-without-value-more-values.xml"    , "ERP-8395-primary-without-value-more-values.json"},
    SampleFiles{"ERP-8395-primary-without-value-null-extension.xml" , "ERP-8395-primary-without-value-null-extension.json"},
    SampleFiles{"ERP-8395-primary-without-value-null-value.xml"     , "ERP-8395-primary-without-value-null-value.json"},
    SampleFiles{"ERP-8395-primary-without-value-null-mixed.xml"     , "ERP-8395-primary-without-value-null-mixed.json"},
    SampleFiles{"ERP-10031-basisprofil-de-r4.xml"                   , "ERP-10031-basisprofil-de-r4.json"},
    SampleFiles{"ERP-23311-CodeSystem-HCPCS-all-codes.xml"          , "ERP-23311-CodeSystem-HCPCS-all-codes.json"}
    // clang-format on
));
