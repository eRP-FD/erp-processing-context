// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

class EuFixture : public EndpointHandlerTest
{
public:
    void checkEuAccessAuthorizationResponse(const ServerResponse& serverResponse,
                                            const ResourceTemplates::EuAccessPermissionOptions& options);
    void giveConsent(const std::string& kvnr);
    void allowCountry(const std::string& country);
    void grant(const std::string& kvnr, std::string accessCode = "aaaaaa");
};
