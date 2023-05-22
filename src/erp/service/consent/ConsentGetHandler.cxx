/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
    const model::Kvnr kvnr{*kvnrClaim, model::Kvnr::Type::pkv};

    const auto consent = session.database()->retrieveConsent(kvnr);
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
        bundle.setTotalSearchMatches(1);
    }

    makeResponse(session, HttpStatus::OK, &bundle);
}
