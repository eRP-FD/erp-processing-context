/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/model/Consent.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/util/TLog.hxx"


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
    const model::Kvnr kvnr{*kvnrClaim};

    // optional category query parameter
    UrlArguments urlArguments({{"category", SearchParameter::Type::String}});
    urlArguments.parse(session.request, session.serviceContext.getKeyDerivation());
    const auto consents = session.database()->retrieveAllConsents(kvnr, urlArguments);
    A_22160.finish();

    model::Bundle bundle(model::BundleType::searchset, model::FhirResourceBase::NoProfile);
    bundle.setTotalSearchMatches(consents.size());
    const auto links = urlArguments.createBundleLinks(getLinkBase(), "/Consent", consents.size());
    for (const auto& link : links)
    {
        bundle.setLink(link.first, link.second);
    }
    for (const auto & consent : consents)
    {
        Expect3(consent.id().has_value(), "Consent id must be set", std::logic_error);
        const auto linkBase = getLinkBase() + "/Consent";
        bundle.addResource(
            linkBase + "/" + std::string(consent.id().value()),
            {},
            model::Bundle::SearchMode::match,
            consent.jsonDocument());
    }

    makeResponse(session, HttpStatus::OK, &bundle);
}
