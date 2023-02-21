/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
    const auto gematikVersion{model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()};
    const auto csFlowtype = (gematikVersion < model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0)
                            ? model::resource::code_system::deprecated::flowType
                            : model::resource::code_system::flowType;
    const std::string createRequest = R"--(<Parameters xmlns="http://hl7.org/fhir">
  <parameter>
    <name value="workflowType"/>
    <valueCoding>
      <system value=")--" + std::string(csFlowtype) + R"--("/>
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

