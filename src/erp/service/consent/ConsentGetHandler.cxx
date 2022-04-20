/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/util/TLog.hxx"


ConsentGetHandler::ConsentGetHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::GET_Consent, allowedProfessionOiDs)
{
}

void ConsentGetHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22160.start("Filter Consent according to kvnr of insurant from access token");
    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "JWT does not contain Kvnr");

    const auto consent = session.database()->retrieveConsent(kvnrClaim.value());
    A_22160.finish();

    model::Bundle bundle(model::BundleType::searchset, model::ResourceBase::NoProfile);
    if(consent.has_value())
    {
        Expect3(consent.value().id().has_value(), "Consent id must be set", std::logic_error);
        const auto linkBase = getLinkBase() + "/Consent";
        bundle.addResource(
            linkBase + "/" + std::string(consent.value().id().value()),
            {},
            model::Bundle::SearchMode::match,
            consent.value().jsonDocument());
    }

    makeResponse(session, HttpStatus::OK, &bundle);
}

