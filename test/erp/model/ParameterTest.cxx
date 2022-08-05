/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Binary.hxx"
#include "erp/model/Parameters.hxx"

#include <gtest/gtest.h>

TEST(ParameterTest, CanGetData)
{
    const std::string xml = R"(
    <Parameters xmlns="http://hl7.org/fhir">
      <parameter>
        <name value="ErxBinary" />
        <resource>
            <Binary>
            <contentType value="application/pkcs7-mime" />
            <data value="MIJVqgYJKoZIhvcNAQcCoIJVmzCCVZcCAQExDzAN..." />
            </Binary>
        </resource>
      </parameter>
    </Parameters>)";

    const auto parameter = model::Parameters::fromXmlNoValidation(xml);
    ASSERT_EQ(parameter.count(), 1);
    const auto& erxBinary = parameter.findResourceParameter("ErxBinary");
    ASSERT_TRUE(erxBinary);
    const auto& binary = model::Binary::fromJson(*erxBinary);
    const auto data = binary.data();
    ASSERT_TRUE(data.has_value());
    ASSERT_EQ(data, "MIJVqgYJKoZIhvcNAQcCoIJVmzCCVZcCAQExDzAN...");
}

TEST(ParameterTest, CanGetChargeItemMarkingFlag)
{
    const std::string json = R"^(
    {
      "resourceType": "Parameters",
      "parameter": [
        {
          "name": "operation",
          "part": [
            {
              "name": "type",
              "valueCode": "add"
            },
            {
              "name": "path",
              "valueString": "ChargeItem.extension('https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_MarkingFlag').extension('taxOffice')"
            },
            {
              "name": "name",
              "valueString": "valueBoolean"
            },
            {
              "name": "value",
              "valueBoolean": true
            }
          ]
        },
        {
          "name": "operation",
          "part": [
            {
              "name": "type",
              "valueCode": "add"
            },
            {
              "name": "path",
              "valueString": "ChargeItem.extension('https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_MarkingFlag').extension('insuranceProvider')"
            },
            {
              "name": "name",
              "valueString": "valueBoolean"
            },
            {
              "name": "value",
              "valueBoolean": false
            }
          ]
        }
      ]
    })^";

    const auto parameter = model::Parameters::fromJsonNoValidation(json);
    ASSERT_EQ(parameter.count(), 2);

    std::optional<model::ChargeItemMarkingFlag> chargeItemMarkingFlag{};
    ASSERT_NO_THROW(chargeItemMarkingFlag = parameter.getChargeItemMarkingFlag());
    ASSERT_TRUE(chargeItemMarkingFlag.has_value());

    auto markings = chargeItemMarkingFlag->getAllMarkings();
    ASSERT_EQ(markings.size(), 2);
    ASSERT_TRUE(markings.count("insuranceProvider"));
    ASSERT_TRUE(markings.count("taxOffice"));
    ASSERT_FALSE(markings["insuranceProvider"]);
    ASSERT_TRUE(markings["taxOffice"]);
}
