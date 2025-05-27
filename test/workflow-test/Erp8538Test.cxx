/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

struct Erp8538TestParams {
    std::string operation;
    std::function<JWT()> jwtBuilder;
};
std::ostream& operator<<(std::ostream& os, const Erp8538TestParams& p)
{
    os << p.operation;
    return os;
}

class Erp8538Test : public ErpWorkflowTestTemplate<testing::TestWithParam<Erp8538TestParams>>
{
};

TEST_P(Erp8538Test, PostOperationWithInvalidPrescriptionId)
{
    std::string abortPath = "/Task/1.0./" + GetParam().operation;
    RequestArguments requestArguments{HttpMethod::POST, abortPath, {}};
    auto jwt = GetParam().jwtBuilder();
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.jwt = jwt;
    requestArguments.overrideExpectedInnerOperation = "POST /Task/<id>/" + GetParam().operation;
    requestArguments.overrideExpectedInnerRole = "";
    requestArguments.overrideExpectedKbvVersion = "XXX";
    requestArguments.overrideExpectedWorkflowVersion = "XXX";

    const auto& [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK);
    HttpStatus expected = GetParam().operation == "$accept" ? HttpStatus::Forbidden : HttpStatus::NotFound;
    EXPECT_EQ(innerResponse.getHeader().status(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    x, Erp8538Test,
    testing::Values(Erp8538TestParams{"$abort",
                                      []() { return JwtBuilder::testBuilder().makeJwtVersicherter("X123456788"); }},
                    Erp8538TestParams{"$activate", []() { return JwtBuilder::testBuilder().makeJwtArzt(); }},
                    Erp8538TestParams{"$accept", []() { return JwtBuilder::testBuilder().makeJwtApotheke(); }},
                    Erp8538TestParams{"$close", []() { return JwtBuilder::testBuilder().makeJwtApotheke(); }},
                    Erp8538TestParams{"$reject", []() { return JwtBuilder::testBuilder().makeJwtApotheke(); }}));
