// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/service/eu/PostGetPrescriptionsHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParGetPrescriptionInput.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/network/client/response/ClientResponseReader.hxx"
#include "shared/util/ByteHelper.hxx"

#include <boost/algorithm/string/join.hpp>

namespace
{
model::AuditEventId mapToAuditEventId(model::GemErpEuPrParGetPrescriptionInput::RequestType requestType)
{
    switch (requestType)
    {
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics:
            return model::AuditEventId::POST_GET_EU_PRESCRIPTIONS_DEMOGRAPHICS;
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list:
            return model::AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_LIST;
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval:
            return model::AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_RETRIEVAL;
    }
    ErpFail(HttpStatus::BadRequest, "Invalid request type");
}
bde::UseCase mapToBdeUseCase(model::GemErpEuPrParGetPrescriptionInput::RequestType requestType)
{
    switch (requestType)
    {
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics:
            return bde::GetDemographics_UC_4_19;
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list:
            return bde::GetPrescriptionsList_UC_4_20;
        case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval:
            return bde::GetPrescriptionsRetrieval_UC_4_21;
    }
    ErpFail(HttpStatus::BadRequest, "Invalid request type");
}
}

namespace eu
{

PostGetPrescriptionsHandler::PostGetPrescriptionsHandler(
    const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : TaskHandlerBase(Operation::GET_eu_prescriptions, allowedProfessionOIDs)
{
}

void PostGetPrescriptionsHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << name() << ": request body: " << session.request.getBody();

    try
    {
        A_27060.start("Schemaprüfung (FHIR Validierung)");
        const auto inputParameters = parseAndValidateRequestBody<model::GemErpEuPrParGetPrescriptionInput>(session);
        A_27060.finish();

        // GEMREQ-start A_27582#telematikId
        const auto telematikId = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        // GEMREQ-end A_27582#telematikId
        ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "JWT does not contain TelematikId");

        const auto kvnr = inputParameters.kvnr();
        const auto requestType = inputParameters.requestType();
        session.setBdeUseCase(mapToBdeUseCase(requestType));
        const auto accessCode = inputParameters.accessCode();
        const auto countryCode = inputParameters.countryCode();
        // GEMREQ-start A_27582#prescriptionIds
        const auto prescriptionIds = inputParameters.prescriptionIds();
        // GEMREQ-end A_27582#prescriptionIds
        const auto practitionerRole = inputParameters.practitionerRole();
        const auto practitionerName = inputParameters.practitionerName();
        const auto healthcareFacilityType = inputParameters.healthcareFacilityType();
        const auto pointOfCare = inputParameters.pointOfCare();

        auto* database = session.database();

        A_27061.start("Prüfung Einwilligung für KVNR");
        const auto consent = database->retrieveConsent(kvnr, model::ConsentType::EUDISPCONS);
        ErpExpect(consent, HttpStatus::Forbidden, "Patienteneinwilligung nicht vorhanden");
        A_27061.finish();

        A_27062.start("Prüfung Zugriffsberechtigung");
        auto storedAccessPermission = database->retrieveEuAccessPermission(kvnr);
        ErpExpect(storedAccessPermission.has_value() && storedAccessPermission->isValid(model::Timestamp::now()),
                  HttpStatus::Forbidden, "Access permission not found");
        ErpExpect(storedAccessPermission->getAccessCode() == accessCode &&
                      storedAccessPermission->getCountryCode() == countryCode,
                  HttpStatus::Forbidden, "Access permission not found");
        A_27062.finish();

        UrlArguments search{
            std::vector<SearchParameter>{{"prescription_id", "prescription_id", SearchParameter::Type::PrescriptionId},
                                         {"status", "status", SearchParameter::Type::TaskStatus}}};

        auto queryParameters = session.request.getQueryParameters();

        // we cannot utilize the LIMIT database feature, we need to sort the result by MedicationRequest.authored_on which is encrypted.
        auto count_parameter = extractCountParameter(queryParameters);

        search.parse(queryParameters, session.serviceContext.getKeyDerivation());

        // GEMREQ-start A_27582#prescriptionIdsFilter
        if (! prescriptionIds.empty())
        {
            search.addHiddenSearchArgument({SearchArgument::Prefix::EQ,
                                            "prescription_id",
                                            "prescription_id",
                                            SearchParameter::Type::PrescriptionId,
                                            prescriptionIds,
                                            {""}});
            // GEMREQ-end A_27582#prescriptionIdsFilter

            A_27589.start("Filter Status - Abfrage nach Liste Rezept-Ids");
            search.addHiddenSearchArgument({SearchArgument::Prefix::EQ,
                                            "status",
                                            "status",
                                            SearchParameter::Type::TaskStatus,
                                            std::vector{model::Task::Status::ready, model::Task::Status::inprogress},
                                            {""}});
            A_27589.finish();
        }
        else
        {
            A_27587.start("Filter Status - Abfrage der aktuellsten Verordnungsinformationen");
            A_27588.start("Filter Status - Abfrage aller einlösbaren Verordnungsinformationen");
            search.addHiddenSearchArgument({SearchArgument::Prefix::EQ,
                                            "status",
                                            "status",
                                            SearchParameter::Type::TaskStatus,
                                            std::vector{model::Task::Status::ready},
                                            {""}});
            A_27587.finish();
            A_27588.finish();
        }


        A_27065.start("Abfrage der aktuellsten Verordnungsinformationen");
        A_27066.start("Abfrage aller einlösbaren Verordnungsinformationen");
        A_27067.start("Abfrage nach Liste Rezept-Ids");
        auto euTasksRaw = database->retrieveAllTasksForEu(kvnr, search);
        ErpExpect(! euTasksRaw.empty(), HttpStatus::NotFound, "Keine einlösbaren Rezepte gefunden");
        A_27065.finish();
        A_27066.finish();
        A_27067.finish();

        A_27064.start("absteigend sortiert nach dem medicationrequest.authored-on Datum");
        auto euTasks = decodeAndSort(euTasksRaw);
        A_27064.finish();

        model::Bundle responseBundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);

        std::size_t totalSearchMatches = 0;
        for (auto& [task, kbvBundle] : euTasks)
        {
            if (count_parameter.has_value() && totalSearchMatches >= count_parameter.value())
            {
                break;
            }
            A_27063_01.start("Filter einlösbarer E-Rezepte");
            const auto [mvoPeriodStartValid, _] = processMvoPeriodStartValid(session, kbvBundle);
            if (! task.isEuRedeemableByProperties() || ! task.isEuRedeemableByPatientAuthorization() ||
                task.kvnr() != kvnr || task.expired() || ! mvoPeriodStartValid)
            {
                continue;
            }
            A_27063_01.finish();

            A_27589.start("Filter Status - Abfrage nach Liste Rezept-Ids");
            if (task.status() != model::Task::Status::ready &&
                (task.status() != model::Task::Status::inprogress || task.owner() != *telematikId))
            {
                continue;
            }
            A_27589.finish();

            ++totalSearchMatches;
            // GEMREQ-start A_27582#prescriptionIdsNotEmpty
            if (! prescriptionIds.empty())
            {
                A_27580.start("Statuswechsel Task");
                task.setStatus(model::Task::Status::inprogress);
                A_27580.finish();
                A_27581.start("Secret");
                // GEMREQ-start A_27581#secret
                const auto secret = SecureRandomGenerator::generate(32);
                task.setSecret(ByteHelper::toHex(secret));
                // GEMREQ-end A_27581#secret
                A_27581.finish();
                A_27582.start("Task Owner");
                // GEMREQ-start A_27582#setOwner
                task.setOwner(*telematikId);
                // GEMREQ-end A_27582#setOwner
                A_27582.finish();
                task.updateLastUpdate();
                database->updateTaskStatusAndSecret(task);
            }
            // GEMREQ-end A_27582#prescriptionIdsNotEmpty

            A_27064.start("add to response bundle");
            responseBundle.addResource(makeFullUrl("/Task/" + task.prescriptionId().toString()), {}, {},
                                       kbvBundle.jsonDocument());

            A_27064.finish();
        }

        ErpExpect(totalSearchMatches > 0, HttpStatus::NotFound, "Keine einlösbaren Rezepte gefunden");

        auto links = search.createBundleLinks(getLinkBase(), "/$get-eu-prescriptions", totalSearchMatches);
        responseBundle.setLink(model::Link::Type::Self, links[model::Link::Type::Self]);

        makeResponse(session, HttpStatus::OK, &responseBundle);

        // Collect Audit data
        session.auditDataCollector()
            .setEventId(mapToAuditEventId(requestType))
            .setInsurantKvnr(kvnr)
            .setAction(prescriptionIds.empty() ? model::AuditEvent::Action::read : model::AuditEvent::Action::update)
            .setCountryCode(countryCode)
            .setAgentName(std::string{practitionerRole} + " " + std::string{practitionerName})
            .setVariable("hftpoc", std::string{healthcareFacilityType} + " " + std::string{pointOfCare})
            .setCountryCode(countryCode);
    }
    catch (const model::ModelException& e)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing/validation error", e.what());
    }
}

std::optional<size_t>
PostGetPrescriptionsHandler::extractCountParameter(std::vector<std::pair<std::string, std::string>>& queryParameters)
{
    try
    {
        auto count_parameter = std::ranges::find_if(queryParameters, [](const auto& param) {
            return param.first == "_count";
        });
        if (count_parameter != queryParameters.end())
        {
            auto value = std::stoul(count_parameter->second);
            queryParameters.erase(count_parameter);
            return value;
        }
        return std::nullopt;
    }
    catch (const std::exception& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "query parameter _count parsing error", ex.what());
    }
}

std::vector<std::tuple<model::Task, model::KbvBundle>>
PostGetPrescriptionsHandler::decodeAndSort(std::vector<std::tuple<model::Task, model::Binary>>& raw)
{
    std::vector<std::tuple<model::Task, model::KbvBundle>> result;
    for (auto& [task, prescriptionBinary] : raw)
    {
        const auto cadesBesSignature = SignedPrescription::fromBinNoVerify(std::string{*prescriptionBinary.data()});
        const auto& bundleXml = cadesBesSignature.payload();
        auto kbvBundle = model::KbvBundle::fromXmlNoValidation(bundleXml);
        result.emplace_back(std::move(task), std::move(kbvBundle));
    }
    std::ranges::stable_sort(result, [](const auto& lhs, const auto& rhs) {
        const auto& kbvBundleLhs = std::get<1>(lhs);
        const auto& kbvBundleRhs = std::get<1>(rhs);
        return kbvBundleLhs.authoredOn() > kbvBundleRhs.authoredOn();
    });
    return result;
}

}
