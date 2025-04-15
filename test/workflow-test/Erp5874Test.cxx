/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

class Erp5874Test : public ErpWorkflowTest
{
public:

};


TEST_F(Erp5874Test, run)
{
    const std::string createRequest = R"--(<Parameters xmlns="http://hl7.org/fhir">
  <parameter>
    <name value="workflowType"/>
    <valueCoding>
      <system value=")--" + std::string(model::resource::code_system::flowType) +
                                      R"--("/>
      <code value="160"/>
    </valueCoding>
  </parameter>
</Parameters>)--";

    ClientResponse innerResponse;
    ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, innerResponse) =
                    send(RequestArguments{HttpMethod::POST,
                                          "/Task/$create",
                                          createRequest,
                                          ContentMimeType::fhirXmlUtf8}
                                          .withJwt(jwtArzt())));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Created);
}
