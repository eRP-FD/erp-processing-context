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
