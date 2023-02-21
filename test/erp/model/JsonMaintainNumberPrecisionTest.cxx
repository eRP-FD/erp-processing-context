/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/erp/model/JsonMaintainNumberPrecisionTest.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/FileHelper.hxx"

#include <boost/format.hpp>

using namespace model;
using namespace model::resource;

namespace rj = rapidjson;

namespace {
    std::string to_string(double value, int precision)
    {
        std::ostringstream oss;
        oss.precision(precision);
        oss.setf(std::ostringstream::showpoint);
        oss.setf(std::ios_base::scientific);
        oss << value;
        return oss.str();
    }
}

std::string JsonMaintainNumberPrecisionTest::createJsonStringCommunicationInfoReq()
{
    // To be able to compare the json input string parsed by the document with
    // the serialized string of the ParseNumberAsStrings document the input string
    // is neither read from a file nor created using a hard coded resource string.
    // Both methods would insert line feeds into the input string.

    rj::StringBuffer s;
    rj::Writer<rj::StringBuffer> writer(s);

    writer.StartObject();
        writer.Key(elements::resourceType);
        writer.String("Communication");
        writer.Key(elements::meta);
        writer.StartObject();
            writer.Key(elements::profile);
            writer.StartArray();
                writer.String(structure_definition::communicationInfoReq.data());
            writer.EndArray();
        writer.EndObject();
        writer.Key(elements::contained);
        writer.StartArray();
            writer.StartObject();
                writer.Key(elements::resourceType);
                writer.String("Medication");
                writer.Key(elements::id);
                writer.String("5fe6e06c-8725-46d5-aecd-e65e041ca3de");
                writer.Key(elements::meta);
                writer.StartObject();
                    writer.Key(elements::profile);
                    writer.StartArray();
                        writer.String("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.00.000");
                    writer.EndArray();
                writer.EndObject();
                writer.Key(elements::extension);
                writer.StartArray();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category");
                        writer.Key(elements::valueCoding);
                        writer.StartObject();
                            writer.Key(elements::system);
                            writer.String("https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category");
                            writer.Key(elements::code);
                            writer.String("00");
                        writer.EndObject();
                    writer.EndObject();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine");
                        writer.Key(elements::valueBoolean);
                        writer.Bool(false);
                    writer.EndObject();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("http://fhir.de/StructureDefinition/normgroesse");
                        writer.Key(elements::valueCode);
                        writer.String("N1");
                    writer.EndObject();
                writer.EndArray();
                writer.Key(elements::code);
                writer.StartObject();
                    writer.Key(elements::coding);
                    writer.StartArray();
                        writer.StartObject();
                            writer.Key(elements::system);
                            writer.String("http://fhir.de/CodeSystem/ifa/pzn");
                            writer.Key(elements::code);
                            writer.String("06313728");
                        writer.EndObject();
                    writer.EndArray();
                    writer.Key(elements::text);
                    writer.String("Sumatriptan-1a Pharma 100 mg Tabletten");
                writer.EndObject();
                writer.Key(elements::form);
                writer.StartObject();
                    writer.Key(elements::coding);
                    writer.StartArray();
                        writer.StartObject();
                            writer.Key(elements::system);
                            writer.String("https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM");
                            writer.Key(elements::code);
                            writer.String("TAB");
                        writer.EndObject();
                    writer.EndArray();
                    writer.Key(elements::text);
                    writer.String("Sumatriptan-1a Pharma 100 mg Tabletten");
                writer.EndObject();
                writer.Key(elements::amount);
                writer.StartObject();
                    writer.Key(elements::numerator);
                    writer.StartObject();
                        writer.Key(elements::value);
                        writer.Int(12);
                        writer.Key(elements::unit);
                        writer.String("TAB");
                        writer.Key(elements::system);
                        writer.String("http://unitsofmeasure.org");
                        writer.Key(elements::code);
                        writer.String("{tbl}");
                    writer.EndObject();
                    writer.Key(elements::denominator);
                    writer.StartObject();
                        writer.Key(elements::value);
                        writer.Int(1);
                    writer.EndObject();
                writer.EndObject();
            writer.EndObject();
        writer.EndArray();
        writer.Key(elements::status);
        writer.String("unknown");
        writer.Key(elements::about);
        writer.StartArray();
            writer.StartObject();
                writer.Key(elements::reference);
                writer.String("#5fe6e06c-8725-46d5-aecd-e65e041ca3de");
            writer.EndObject();
        writer.EndArray();
        writer.Key(elements::recipient);
        writer.StartArray();
            writer.StartObject();
                writer.Key(elements::identifier);
                writer.StartObject();
                    writer.Key(elements::system);
                    writer.String(std::string(naming_system::deprecated::telematicID));
                    writer.Key(elements::value);
                    writer.String("606358757");
                writer.EndObject();
            writer.EndObject();
        writer.EndArray();
        writer.Key(elements::payload);
        writer.StartArray();
            writer.StartObject();
                writer.Key(elements::extension);
                writer.StartArray();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://gematik.de/fhir/StructureDefinition/InsuranceProvider");
                        writer.Key(elements::valueIdentifier);
                        writer.StartObject();
                            writer.Key(elements::system);
                            writer.String("http://fhir.de/NamingSystem/arge-ik/iknr");
                            writer.Key(elements::value);
                            writer.String("104212059");
                        writer.EndObject();
                    writer.EndObject();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://gematik.de/fhir/StructureDefinition/SupplyOptionsType");
                        writer.Key(elements::extension);
                        writer.StartArray();
                            writer.StartObject();
                                writer.Key(elements::url);
                                writer.String("onPremise");
                                writer.Key(elements::valueBoolean);
                                writer.Bool(true);
                            writer.EndObject();
                            writer.StartObject();
                                writer.Key(elements::url);
                                writer.String("delivery");
                                writer.Key(elements::valueBoolean);
                                writer.Bool(true);
                            writer.EndObject();
                            writer.StartObject();
                                writer.Key(elements::url);
                                writer.String("shipment");
                                writer.Key(elements::valueBoolean);
                                writer.Bool(false);
                            writer.EndObject();
                        writer.EndArray();
                    writer.EndObject();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://gematik.de/fhir/StructureDefinition/SubstitutionAllowedType");
                        writer.Key(elements::valueBoolean);
                        writer.Bool(true);
                    writer.EndObject();
                    writer.StartObject();
                        writer.Key(elements::url);
                        writer.String("https://gematik.de/fhir/StructureDefinition/PrescriptionType");
                        writer.Key(elements::valueCoding);
                        writer.StartObject();
                            writer.Key(elements::system);
                            writer.String("https://gematik.de/fhir/CodeSystem/Flowtype");
                            writer.Key(elements::code);
                            writer.String("160");
                            writer.Key(elements::display);
                            writer.String("Muster 16 (Apothekenpflichtige Arzneimittel)");
                        writer.EndObject();
                    writer.EndObject();
                writer.EndArray();
                writer.Key(elements::contentString);
                writer.String("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.");
            writer.EndObject();
        writer.EndArray();
    writer.EndObject(); // root

    return std::string(s.GetString(), s.GetLength());
}

TEST_F(JsonMaintainNumberPrecisionTest, EmptyString)
{
    std::string jsonString;
    EXPECT_THROW(JsonMaintainNumberPrecisionTestModel::fromJson(jsonString), ModelException);
}

TEST_F(JsonMaintainNumberPrecisionTest, InvalidDocument)
{
    std::string jsonString = R"({{})";
    EXPECT_THROW(JsonMaintainNumberPrecisionTestModel::fromJson(jsonString), ModelException);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootObjectEmpty)
{
    std::string jsonStringIn = R"({})";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootArrayEmpty)
{
    std::string jsonStringIn = R"([])";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootObjectOneMember)
{
    const rj::Pointer profilePointer(ElementName::path("profile"));
    std::string jsonStringIn = R"({"profile":"https://gematik.de/ErxCommunicationInfoReq"})";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(profilePointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(profilePointer), "https://gematik.de/ErxCommunicationInfoReq");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootArrayOneElement)
{
    const rj::Pointer element0Pointer(ElementName::path(0));
    std::string jsonStringIn = R"(["https://gematik.de/ErxCommunicationInfoReq"])";;
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(element0Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element0Pointer), "https://gematik.de/ErxCommunicationInfoReq");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootObjectTopLevelMembers)//NOLINT(readability-function-cognitive-complexity)
{
    const rj::Pointer resourceTypePointer(ElementName::path("resourceType"));
    const rj::Pointer metaPointer(ElementName::path("meta"));
    const rj::Pointer profilePointer(ElementName::path("profile"));
    std::string jsonStringIn = R"({"resourceType":"TestModel","meta":"StringValue","profile":"https://gematik.de/ErxCommunicationInfoReq"})";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(resourceTypePointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(resourceTypePointer), "TestModel");
    ASSERT_NE(metaPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(metaPointer), "StringValue");
    ASSERT_NE(profilePointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(profilePointer), "https://gematik.de/ErxCommunicationInfoReq");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootArrayTopLevelElements)//NOLINT(readability-function-cognitive-complexity)
{
    const rj::Pointer element0Pointer(ElementName::path(0));
    const rj::Pointer element1Pointer(ElementName::path(1));
    const rj::Pointer element2Pointer(ElementName::path(2));
    std::string jsonStringIn = R"(["TestModel","StringValue","https://gematik.de/ErxCommunicationInfoReq"])";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(element0Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element0Pointer), "TestModel");
    ASSERT_NE(element1Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element1Pointer), "StringValue");
    ASSERT_NE(element2Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element2Pointer), "https://gematik.de/ErxCommunicationInfoReq");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootObjectOneTopLevelArrayWithOneNullElement)
{
    std::string jsonStringIn = R"({"contained":[]})";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootArrayOneTopLevelObjectWithEmptyArray)
{
    std::string jsonStringIn = R"([{"contained":[]}])";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootObjectOneTopLevelArrayWithSeveralElements)//NOLINT(readability-function-cognitive-complexity)
{
    const rj::Pointer element0Pointer(ElementName::path(elements::contained, 0));
    const rj::Pointer element1Pointer(ElementName::path(elements::contained, 1));
    const rj::Pointer element2Pointer(ElementName::path(elements::contained, 2));
    std::string jsonStringIn = R"({"contained":[1.0,2.00,3.000]})";;
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(element0Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element0Pointer), "1.0");
    ASSERT_NE(element1Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element1Pointer), "2.00");
    ASSERT_NE(element2Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element2Pointer), "3.000");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, RootArrayOneTopLevelObjectWithSeveralArrayElements)//NOLINT(readability-function-cognitive-complexity)
{
    const rj::Pointer element0Pointer(ElementName::path(0, "contained", 0));
    const rj::Pointer element1Pointer(ElementName::path(0, "contained", 1));
    const rj::Pointer element2Pointer(ElementName::path(0, "contained", 2));
    std::string jsonStringIn = R"([{"contained":[1.0,2.00,3.000]}])";
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    ASSERT_NE(element0Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element0Pointer), "1.0");
    ASSERT_NE(element1Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element1Pointer), "2.00");
    ASSERT_NE(element2Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(element2Pointer), "3.000");
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, CommunicationInfoReq)
{
    std::string jsonStringIn = createJsonStringCommunicationInfoReq();
    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);
}

TEST_F(JsonMaintainNumberPrecisionTest, Numbers)//NOLINT(readability-function-cognitive-complexity)
{
    const rj::Pointer elementNotUsedPointer(ElementName::path("not_used"));
    const rj::Pointer elementStatusPointer(ElementName::path("status"));
    const rj::Pointer elementIntMinPointer(ElementName::path("int_min"));
    const rj::Pointer elementIntMaxPointer(ElementName::path("int_max"));
    const rj::Pointer elementUIntMinPointer(ElementName::path("uint_min"));
    const rj::Pointer elementUIntMaxPointer(ElementName::path("uint_max"));
    const rj::Pointer elementInt64MinPointer(ElementName::path("int64_min"));
    const rj::Pointer elementInt64MaxPointer(ElementName::path("int64_max"));
    const rj::Pointer elementUInt64MinPointer(ElementName::path("uint64_min"));
    const rj::Pointer elementUInt64MaxPointer(ElementName::path("uint64_max"));
    const rj::Pointer elementDblMinPointer(ElementName::path("dbl_min"));
    const rj::Pointer elementDblMaxPointer(ElementName::path("dbl_max"));
    const rj::Pointer elementDbl1Pointer(ElementName::path("dbl1"));
    const rj::Pointer elementDbl2Pointer(ElementName::path("dbl2"));
    const rj::Pointer elementDbl3Pointer(ElementName::path("dbl3"));

    const std::string intMinStr = std::to_string(std::numeric_limits<int>::lowest());
    const std::string intMaxStr = std::to_string(std::numeric_limits<int>::max());
    const std::string uintMinStr = std::to_string(std::numeric_limits<unsigned int>::lowest());
    const std::string uintMaxStr = std::to_string(std::numeric_limits<unsigned int>::max());
    const std::string int64MinStr = std::to_string(std::numeric_limits<int64_t>::lowest());
    const std::string int64MaxStr = std::to_string(std::numeric_limits<int64_t>::max());
    const std::string uint64MinStr = std::to_string(std::numeric_limits<uint64_t>::lowest());
    const std::string uint64MaxStr = std::to_string(std::numeric_limits<uint64_t>::max());
    const std::string dblMinStr = to_string(std::numeric_limits<double>::lowest(), 6);
    const std::string dblMaxStr = to_string(std::numeric_limits<double>::max(), 6);

    std::string jsonStringIn;
    jsonStringIn.append(R"({)");
    jsonStringIn.append(R"("resourceType":"TestModel")");
    jsonStringIn.append(R"(,"status":"unknown")");
    jsonStringIn.append(R"(,"sent":"2020-03-20T07:13:00+05:00")");
    jsonStringIn.append(R"(,"int_min":)" + intMinStr);
    jsonStringIn.append(R"(,"int_max":)" + intMaxStr);
    jsonStringIn.append(R"(,"uint_min":)" + uintMinStr);
    jsonStringIn.append(R"(,"uint_max":)" + uintMaxStr);
    jsonStringIn.append(R"(,"int64_min":)" + int64MinStr);
    jsonStringIn.append(R"(,"int64_max":)" + int64MaxStr);
    jsonStringIn.append(R"(,"uint64_min":)" + uint64MinStr);
    jsonStringIn.append(R"(,"uint64_max":)" + uint64MaxStr);
    jsonStringIn.append(R"(,"dbl_min":)" + dblMinStr);
    jsonStringIn.append(R"(,"dbl_max":)" + dblMaxStr);
    jsonStringIn.append(R"(,"dbl1":1.0)");
    jsonStringIn.append(R"(,"dbl2":2.00)");
    jsonStringIn.append(R"(,"dbl3":3.000)");
    jsonStringIn.append(R"(})");

    JsonMaintainNumberPrecisionTestModel model = JsonMaintainNumberPrecisionTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToJsonString();
    EXPECT_EQ(jsonStringIn, jsonStringOut);

    ASSERT_EQ(elementNotUsedPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_THROW((void)model.jsonDocument().getStringValueFromPointer(elementNotUsedPointer), ModelException);

    ASSERT_NE(elementStatusPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementStatusPointer), "unknown");
    ASSERT_NE(elementIntMinPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementIntMinPointer), intMinStr);
    ASSERT_NE(elementIntMaxPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementIntMaxPointer), intMaxStr);
    ASSERT_NE(elementUIntMinPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementUIntMinPointer), uintMinStr);
    ASSERT_NE(elementUIntMaxPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementUIntMaxPointer), uintMaxStr);
    ASSERT_NE(elementInt64MinPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementInt64MinPointer), int64MinStr);
    ASSERT_NE(elementInt64MaxPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementInt64MaxPointer), int64MaxStr);
    ASSERT_NE(elementUInt64MinPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementUInt64MinPointer), uint64MinStr);
    ASSERT_NE(elementUInt64MaxPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementUInt64MaxPointer), uint64MaxStr);
    ASSERT_NE(elementDblMinPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementDblMinPointer), dblMinStr);
    ASSERT_NE(elementDblMaxPointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementDblMaxPointer), dblMaxStr);
    ASSERT_NE(elementDbl1Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementDbl1Pointer), "1.0");
    ASSERT_NE(elementDbl2Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementDbl2Pointer), "2.00");
    ASSERT_NE(elementDbl3Pointer.Get(model.jsonDocument()), nullptr);
    EXPECT_EQ(model.jsonDocument().getStringValueFromPointer(elementDbl3Pointer), "3.000");

    EXPECT_FALSE(model.jsonDocument().getOptionalStringValue(elementNotUsedPointer).has_value());
    EXPECT_FALSE(model.jsonDocument().getOptionalIntValue(elementNotUsedPointer).has_value());
    EXPECT_FALSE(model.jsonDocument().getOptionalUIntValue(elementNotUsedPointer).has_value());
    EXPECT_FALSE(model.jsonDocument().getOptionalInt64Value(elementNotUsedPointer).has_value());
    EXPECT_FALSE(model.jsonDocument().getOptionalUInt64Value(elementNotUsedPointer).has_value());
    EXPECT_FALSE(model.jsonDocument().getOptionalDoubleValue(elementNotUsedPointer).has_value());

    EXPECT_THROW((void)model.jsonDocument().getOptionalStringValue(elementIntMinPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalIntValue(elementStatusPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalUIntValue(elementStatusPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalInt64Value(elementStatusPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalUInt64Value(elementStatusPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalDoubleValue(elementStatusPointer), ModelException);
    EXPECT_THROW((void)model.jsonDocument().getOptionalDoubleValue(elementStatusPointer), ModelException);

    EXPECT_TRUE(model.jsonDocument().getOptionalStringValue(elementStatusPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalStringValue(elementStatusPointer).value(), "unknown");
    EXPECT_TRUE(model.jsonDocument().getOptionalIntValue(elementIntMinPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalIntValue(elementIntMinPointer).value(), std::numeric_limits<int>::lowest());
    EXPECT_TRUE(model.jsonDocument().getOptionalIntValue(elementIntMaxPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalIntValue(elementIntMaxPointer).value(), std::numeric_limits<int>::max());
    EXPECT_TRUE(model.jsonDocument().getOptionalUIntValue(elementUIntMinPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalUIntValue(elementUIntMinPointer).value(), std::numeric_limits<unsigned int>::lowest());
    EXPECT_TRUE(model.jsonDocument().getOptionalUIntValue(elementUIntMaxPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalUIntValue(elementUIntMaxPointer).value(), std::numeric_limits<unsigned int>::max());
    EXPECT_TRUE(model.jsonDocument().getOptionalInt64Value(elementInt64MinPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalInt64Value(elementInt64MinPointer).value(), std::numeric_limits<int64_t>::lowest());
    EXPECT_TRUE(model.jsonDocument().getOptionalInt64Value(elementInt64MaxPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalInt64Value(elementInt64MaxPointer).value(), std::numeric_limits<int64_t>::max());
    EXPECT_TRUE(model.jsonDocument().getOptionalUInt64Value(elementUInt64MinPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalUInt64Value(elementUInt64MinPointer).value(), std::numeric_limits<uint64_t>::lowest());
    EXPECT_TRUE(model.jsonDocument().getOptionalUInt64Value(elementUInt64MaxPointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalUInt64Value(elementUInt64MaxPointer).value(), std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(model.jsonDocument().getOptionalDoubleValue(elementDblMinPointer).has_value());
    EXPECT_EQ(to_string(model.jsonDocument().getOptionalDoubleValue(elementDblMinPointer).value(), 1), to_string(std::numeric_limits<double>::lowest(), 1));
    EXPECT_TRUE(model.jsonDocument().getOptionalDoubleValue(elementDblMaxPointer).has_value());
    EXPECT_EQ(to_string(model.jsonDocument().getOptionalDoubleValue(elementDblMaxPointer).value(), 1), to_string(std::numeric_limits<double>::max(), 1));
    EXPECT_TRUE(model.jsonDocument().getOptionalDoubleValue(elementDbl1Pointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalDoubleValue(elementDbl1Pointer).value(), 1.0);
    EXPECT_TRUE(model.jsonDocument().getOptionalDoubleValue(elementDbl2Pointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalDoubleValue(elementDbl2Pointer).value(), 2.0);
    EXPECT_TRUE(model.jsonDocument().getOptionalDoubleValue(elementDbl3Pointer).has_value());
    EXPECT_EQ(model.jsonDocument().getOptionalDoubleValue(elementDbl3Pointer).value(), 3.0);

    EXPECT_THROW((void)model.getValueAsString("not_used"), ModelException);
    EXPECT_THROW((void)model.getValueAsInt("not_used"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt("not_used"), ModelException);
    EXPECT_THROW((void)model.getValueAsInt64("not_used"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt64("not_used"), ModelException);

    EXPECT_THROW((void)model.getValueAsString("int_min"), ModelException);
    EXPECT_THROW((void)model.getValueAsInt("status"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt("status"), ModelException);
    EXPECT_THROW((void)model.getValueAsInt64("status"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt64("status"), ModelException);
    EXPECT_THROW((void)model.getValueAsDouble("status"), ModelException);
    EXPECT_THROW((void)model.getValueAsDouble("status"), ModelException);

    EXPECT_EQ(model.getValueAsString("status"), "unknown");
    EXPECT_EQ(model.getValueAsInt("int_min"), std::numeric_limits<int>::lowest());
    EXPECT_EQ(model.getValueAsInt("int_max"), std::numeric_limits<int>::max());
    EXPECT_EQ(model.getValueAsUInt("uint_min"), std::numeric_limits<unsigned int>::lowest());
    EXPECT_EQ(model.getValueAsUInt("uint_max"), std::numeric_limits<unsigned int>::max());
    EXPECT_EQ(model.getValueAsInt64("int64_min"), std::numeric_limits<int64_t>::lowest());
    EXPECT_EQ(model.getValueAsInt64("int64_max"), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(model.getValueAsUInt64("uint64_min"), std::numeric_limits<uint64_t>::lowest());
    EXPECT_EQ(model.getValueAsUInt64("uint64_max"), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(to_string(model.getValueAsDouble("dbl_min"), 1), to_string(std::numeric_limits<double>::lowest(), 1));
    EXPECT_EQ(to_string(model.getValueAsDouble("dbl_max"), 1), to_string(std::numeric_limits<double>::max(), 1));
    EXPECT_EQ(model.getValueAsDouble("dbl1"), 1.0);
    EXPECT_EQ(model.getValueAsDouble("dbl2"), 2.0);
    EXPECT_EQ(model.getValueAsDouble("dbl3"), 3.0);

    EXPECT_THROW((void)model.getValueAsInt("int64_max"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt("int64_min"), ModelException);
    EXPECT_THROW((void)model.getValueAsInt64("dbl_max"), ModelException);
    EXPECT_THROW((void)model.getValueAsUInt64("dbl_min"), ModelException);

    model.setValue("status", "entered_in_error");
    model.setValue("int_-42", -42);
    model.setValue("uint_42", 42);
    model.setValue("int64_-42", static_cast<int64_t>(-42));
    model.setValue("uint64_42", static_cast<uint64_t>(42));
    model.setValue("double_1_1", 1.1);

    EXPECT_EQ(model.getValueAsString("status"), "entered_in_error");
    EXPECT_EQ(model.getValueAsInt("int_-42"), -42);
    EXPECT_EQ(model.getValueAsUInt("uint_42"), static_cast<unsigned int>(42));
    EXPECT_EQ(model.getValueAsInt64("int64_-42"), static_cast<int64_t>(-42));
    EXPECT_EQ(model.getValueAsUInt64("uint64_42"), static_cast<uint64_t>(42));
    EXPECT_EQ(model.getValueAsDouble("double_1_1"), 1.1);
}

TEST_F(JsonMaintainNumberPrecisionTest, ArrayHelpers)//NOLINT(readability-function-cognitive-complexity)
{
    std::string jsonString = R"({
        "resourceType":"TestModel",
        "status":"unknown",
        "sent":"2020-03-20T07:13:00+05:00",
        "entry": [
            {
                "fullUrl": "http://pvs.praxis-topp-gluecklich.local/0",
                "resource": {
                    "resourceType": "Composition",
                    "id": "ed52c1e3-b700-4497-ae19-b23744e29876",
                    "amount": 1.0340
                }
            },
            {
                "fullUrl": "http://pvs.praxis-topp-gluecklich.local/1",
                "resource": {
                    "resourceType": "MedicationRequest",
                    "id": "e930cdee-9eb5-4b44-88b5-2a18b69f3b9a",
                    "amount": 2.0340
                }
            }
        ]
    })";

    NumberAsStringParserDocument doc = NumberAsStringParserDocument::fromJson(jsonString);

    const rj::Pointer entryArrayPointer(ElementName::path("entry"));
    const rj::Pointer searchKey(ElementName::path("resource", "resourceType"));

    // findStringInArray -> string_view
    {
        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        std::optional<std::string_view> id =
            doc.findStringInArray(entryArrayPointer, searchKey, "Composition", resourceIdPointer);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id.value(), "ed52c1e3-b700-4497-ae19-b23744e29876");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        std::optional<std::string_view> amount =
            doc.findStringInArray(entryArrayPointer, searchKey, "Composition", resourceAmountPointer);
        ASSERT_TRUE(amount.has_value());
        EXPECT_EQ(amount.value(), "1.0340");
    }
    {
        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        std::optional<std::string_view> id =
            doc.findStringInArray(entryArrayPointer, searchKey, "MedicationRequest", resourceIdPointer);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id.value(), "e930cdee-9eb5-4b44-88b5-2a18b69f3b9a");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        std::optional<std::string_view> amount =
            doc.findStringInArray(entryArrayPointer, searchKey, "MedicationRequest", resourceAmountPointer);
        ASSERT_TRUE(amount.has_value());
        EXPECT_EQ(amount.value(), "2.0340");
    }

    const rj::Pointer resourceObjectPointer(ElementName::path("resource"));

    // findMemberInArray -> memberAndPosition
    {
        std::optional<std::tuple<const rj::Value*, std::size_t>> memberAndPosition =
            doc.findMemberInArray(entryArrayPointer, searchKey, "Composition", resourceObjectPointer);

        ASSERT_TRUE(memberAndPosition.has_value());
        const rj::Value* member = std::get<0>(memberAndPosition.value());
        const std::size_t position = std::get<1>(memberAndPosition.value());
        EXPECT_TRUE(member->IsObject());
        EXPECT_EQ(position, 0);

        const rj::Pointer resourceTypePointer(ElementName::path("resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "Composition");

        const rj::Pointer idPointer(ElementName::path("id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, idPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, idPointer).value(), "ed52c1e3-b700-4497-ae19-b23744e29876");

        const rj::Pointer amountPointer(ElementName::path("amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, amountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, amountPointer).value(), 1.034);
    }
    {
        std::optional<std::tuple<const rj::Value*, std::size_t>> memberAndPosition =
            doc.findMemberInArray(entryArrayPointer, searchKey, "MedicationRequest", resourceObjectPointer);

        ASSERT_TRUE(memberAndPosition.has_value());
        const rj::Value* member = std::get<0>(memberAndPosition.value());
        const std::size_t position = std::get<1>(memberAndPosition.value());
        EXPECT_TRUE(member->IsObject());
        EXPECT_EQ(position, 1);

        const rj::Pointer resourceTypePointer(ElementName::path("resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "MedicationRequest");

        const rj::Pointer idPointer(ElementName::path("id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, idPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, idPointer).value(), "e930cdee-9eb5-4b44-88b5-2a18b69f3b9a");

        const rj::Pointer amountPointer(ElementName::path("amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, amountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, amountPointer).value(), 2.034);
    }

    // findMemberInArray -> rj::Value*
    {
        const rj::Value* member = doc.findMemberInArray(entryArrayPointer, searchKey, "Composition");

        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "Composition");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "ed52c1e3-b700-4497-ae19-b23744e29876");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 1.034);
    }
    {
        const rj::Value* member = doc.findMemberInArray(entryArrayPointer, searchKey, "MedicationRequest");

        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "MedicationRequest");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "e930cdee-9eb5-4b44-88b5-2a18b69f3b9a");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 2.034);
    }

    // getMemberInArray -> rj::Value*
    {
        const rj::Value* member = doc.getMemberInArray(entryArrayPointer, 0);

        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "Composition");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "ed52c1e3-b700-4497-ae19-b23744e29876");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 1.034);
    }
    {
        const rj::Value* member = doc.getMemberInArray(entryArrayPointer, 1);

        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "MedicationRequest");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "e930cdee-9eb5-4b44-88b5-2a18b69f3b9a");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 2.034);
    }

    // The medicationTemplate document is added as an object to the "main" document.
    // The template document MUST NOT be destroyed before the "main" document as that
    // would free the referenced strings.
    std::string_view jsonStringMedicationTemplate = R"({
            "fullUrl": "http://pvs.praxis-topp-gluecklich.local/2",
            "resource" : {
                "resourceType": "Medication",
                "id" : "5fe6e06c-8725-46d5-aecd-e65e041ca3de",
                "amount": 3.0340
            }})";
    NumberAsStringParserDocument medicationTemplate = NumberAsStringParserDocument::fromJson(jsonStringMedicationTemplate);

    // addToArray -> size_t
    {
        std::size_t position = doc.addToArray(entryArrayPointer, std::move(medicationTemplate));
        ASSERT_EQ(position, 2);

        const rj::Value* member = doc.getMemberInArray(entryArrayPointer, position);

        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "Medication");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        EXPECT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "5fe6e06c-8725-46d5-aecd-e65e041ca3de");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 3.034);
    }

    // removeFromArray
    {
        // Remove "MedicationRequest" from array.
        doc.removeFromArray(entryArrayPointer, 1);

        const rj::Value* member = doc.getMemberInArray(entryArrayPointer, 1);

        // Medication must now be the second entry in the array.
        ASSERT_NE(member, nullptr);
        EXPECT_TRUE(member->IsObject());

        const rj::Pointer resourceTypePointer(ElementName::path("resource", "resourceType"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceTypePointer).has_value());
        ASSERT_EQ(doc.getOptionalStringValue(*member, resourceTypePointer).value(), "Medication");

        const rj::Pointer resourceIdPointer(ElementName::path("resource", "id"));
        ASSERT_TRUE(doc.getOptionalStringValue(*member, resourceIdPointer).has_value());
        ASSERT_EQ(doc.getOptionalStringValue(*member, resourceIdPointer).value(), "5fe6e06c-8725-46d5-aecd-e65e041ca3de");

        const rj::Pointer resourceAmountPointer(ElementName::path("resource", "amount"));
        ASSERT_TRUE(doc.getOptionalDoubleValue(*member, resourceAmountPointer).has_value());
        EXPECT_EQ(doc.getOptionalDoubleValue(*member, resourceAmountPointer).value(), 3.034);
    }

    // clearArray
    {
        doc.clearArray(entryArrayPointer);
        const rj::Value* member = doc.getMemberInArray(entryArrayPointer, 0);
        ASSERT_EQ(member, nullptr);
    }
}

TEST_F(JsonMaintainNumberPrecisionTest, makeBool)
{
    model::NumberAsStringParserDocument doc;
    doc.SetObject();
    doc.AddMember("valueTrue", doc.makeBool(true), doc.GetAllocator());
    doc.AddMember("valueFalse", doc.makeBool(false), doc.GetAllocator());
    EXPECT_EQ(doc.serializeToJsonString(), R"({"valueTrue":true,"valueFalse":false})");
}

TEST_F(JsonMaintainNumberPrecisionTest, makeNumber)
{
    model::NumberAsStringParserDocument doc;
    doc.SetObject();
    doc.AddMember("noTrailingZero", doc.makeNumber("0.123"), doc.GetAllocator());
    doc.AddMember("trailingZero", doc.makeNumber("0.1230"), doc.GetAllocator());
    doc.AddMember("preciseZero", doc.makeNumber("0.0000"), doc.GetAllocator());
    EXPECT_EQ(doc.serializeToJsonString(), R"({"noTrailingZero":0.123,"trailingZero":0.1230,"preciseZero":0.0000})");
}

TEST_F(JsonMaintainNumberPrecisionTest, makeString)
{
    model::NumberAsStringParserDocument doc;
    doc.SetObject();
    doc.AddMember("aString", doc.makeString("some test string"), doc.GetAllocator());
    doc.AddMember("trailingSpace", doc.makeString("Space-> "), doc.GetAllocator());
    doc.AddMember("leadingSpace", doc.makeString(" <-Space"), doc.GetAllocator());
    EXPECT_EQ(doc.serializeToJsonString(),
              R"({"aString":"some test string","trailingSpace":"Space-> ","leadingSpace":" <-Space"})");
}

TEST_F(JsonMaintainNumberPrecisionTest, validNumbers)//NOLINT(readability-function-cognitive-complexity)
{
    model::NumberAsStringParserDocument doc;
    EXPECT_NO_THROW((void)doc.makeNumber("0"));
    EXPECT_NO_THROW((void)doc.makeNumber("1"));
    EXPECT_NO_THROW((void)doc.makeNumber("-1"));
    EXPECT_NO_THROW((void)doc.makeNumber("1e+1"));
    EXPECT_NO_THROW((void)doc.makeNumber("-1e+1"));
    EXPECT_NO_THROW((void)doc.makeNumber("1e-1"));
    EXPECT_NO_THROW((void)doc.makeNumber("-1e-1"));
    EXPECT_NO_THROW((void)doc.makeNumber("1.0e+1"));
    EXPECT_NO_THROW((void)doc.makeNumber("-1.0e+1"));
    EXPECT_NO_THROW((void)doc.makeNumber("-1.0e-1"));
    EXPECT_NO_THROW((void)doc.makeNumber("1.1"));
    EXPECT_NO_THROW((void)doc.makeNumber("1.0"));
    EXPECT_NO_THROW((void)doc.makeNumber("1.100"));
}

TEST_F(JsonMaintainNumberPrecisionTest, invalidNumbers)//NOLINT(readability-function-cognitive-complexity)
{
    model::NumberAsStringParserDocument doc;
    EXPECT_THROW((void)doc.makeNumber(" 1.100"), ModelException);
    EXPECT_THROW((void)doc.makeNumber("1.100 "), ModelException);
    EXPECT_THROW((void)doc.makeNumber("1 .100"), ModelException);
    EXPECT_THROW((void)doc.makeNumber("1."), ModelException);
    EXPECT_THROW((void)doc.makeNumber("-"), ModelException);
    EXPECT_THROW((void)doc.makeNumber("+"), ModelException);
}
