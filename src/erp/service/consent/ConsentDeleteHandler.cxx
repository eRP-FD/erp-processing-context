/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "erp/model/Consent.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/TLog.hxx"


ConsentDeleteHandler::ConsentDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::DELETE_Consent, allowedProfessionOiDs)
{
}

// GEMREQ-start A_22157
// GEMREQ-start A_22158
void ConsentDeleteHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22154.start("Category must be specified");
    const auto categoryStr = session.request.getQueryParameter("category");
    ErpExpect(categoryStr.has_value(), HttpStatus::MethodNotAllowed, "Category must be specified");
    A_22154.finish();

    A_22874_01.start("Category must have value 'CHARGCONS' or 'EUDISPCONS'");
    auto category = magic_enum::enum_cast<model::ConsentType>(*categoryStr);
    ErpExpect(category.has_value(), HttpStatus::BadRequest, "Category must be 'CHARGCONS' or 'EUDISPCONS'");
    switch (*category)
    {
        case model::ConsentType::CHARGCONS:
            break;
        case model::ConsentType::EUDISPCONS:
            ErpExpect(Configuration::instance().getBoolValue(ConfigurationKey::FEATURE_EU),
                      HttpStatus::MethodNotAllowed, "EU feature is disabled in configuration ERP_FEATURE_EU");
            break;
    }
    A_22874_01.finish();

    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "ACCESS_TOKEN does not contain KVNR");
    const model::Kvnr kvnr{*kvnrClaim};

    auto* databaseHandle = session.database();

    A_22158.start("Delete the consent matched by the KVNR from the ACCESS_TOKEN (CHARGCONS)");
    A_27131.start("Delete the consent matched by the KVNR from the ACCESS_TOKEN (EUDISPCONS)");
    ErpExpect(databaseHandle->clearConsent(kvnr, *category),
              HttpStatus::NotFound,
              "Could not find any consent for given KVNR ");
    A_27131.finish();
    A_22158.finish();
// GEMREQ-end A_22158

    switch (*category)
    {
        case model::ConsentType::CHARGCONS:
            A_22157.start("Delete all charge information and related communication for insurant matched by the KVNR from the ACCESS_TOKEN");
            databaseHandle->clearAllChargeInformation(kvnr);
            databaseHandle->clearAllChargeItemCommunications(kvnr);
            A_22157.finish();
            break;
        case model::ConsentType::EUDISPCONS:
            break;
    }
// GEMREQ-end A_22157

    // Collect Audit data
    const auto getAuditEventId = [](model::ConsentType category) {
        switch (category)
        {
            case model::ConsentType::EUDISPCONS:
                return model::AuditEventId::DELETE_EU_Consent;
            case model::ConsentType::CHARGCONS:
                break;
        }
        return model::AuditEventId::DELETE_Consent;
    };

    session.auditDataCollector()
        .setEventId(getAuditEventId(*category))
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::del);

    session.response.setStatus(HttpStatus::NoContent);
}
