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
    const std::string createRequest =
R"(<Parameters xmlns="http://hl7.org/fhir"><parameter><name value="workflowType"/><valueCoding><system value="https://gematik.de/fhir/CodeSystem/Flowtype"/><code value="160"/></valueCoding></parameter></Parameters>)";

};


TEST_F(Erp5874Test, run)
{
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

