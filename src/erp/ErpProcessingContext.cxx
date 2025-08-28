/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpProcessingContext.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/CommunicationDeleteHandler.hxx"
#include "erp/service/CommunicationGetHandler.hxx"
#include "erp/service/CommunicationPostHandler.hxx"
#include "erp/service/DeviceHandler.hxx"
#include "erp/service/HealthHandler.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/service/MetaDataHandler.hxx"
#include "erp/service/SubscriptionPostHandler.hxx"
#include "erp/service/VauRequestHandler.hxx"
#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"
#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPostHandler.hxx"
#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/service/consent/ConsentPostHandler.hxx"
#include "erp/service/eu/PostGetPrescriptionsHandler.hxx"
#include "erp/service/euAccessPermission/GrantHandler.hxx"
#include "erp/service/euAccessPermission/ReadHandler.hxx"
#include "erp/service/euAccessPermission/RevokeHandler.hxx"
#include "erp/service/task/AbortTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/service/task/RejectTaskHandler.hxx"
#include "erp/service/task/eu/EuCloseTaskHandler.hxx"
#include "erp/service/task/eu/PatchTaskHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/util/Configuration.hxx"

namespace ErpProcessingContext
{

void addPrimaryEndpoints (
    RequestHandlerManager& primaryManager,
    RequestHandlerManager&& secondaryManager)
{
    addSecondaryEndpoints(secondaryManager);
    primaryManager.onPostDo("/VAU/{UP}",
        std::make_unique<VauRequestHandler>(std::move(secondaryManager)));
    primaryManager.onGetDo("/health", std::make_unique<HealthHandler>());
}


void addSecondaryEndpoints (RequestHandlerManager& handlerManager)
{
    using namespace profession_oid;

    using oids = std::initializer_list<std::string_view>;

    // For GET /Task see gemSpec_FD_eRp_V1.1.1, 6.1.1
    A_21558_01.start("Register the allowed professionOIDs");
    handlerManager.onGetDo("/Task",
            std::make_unique<GetAllTasksHandler>(oids{
                    oid_versicherter,
                    oid_oeffentliche_apotheke,
                    oid_krankenhausapotheke}))
        .setErpUseCase(
        std::map<InnerRequestRole, bde::UseCase>{{InnerRequestRole::pharmacy, bde::GetTasksPharmacy_UC_4_12},
                                                 {InnerRequestRole::patient, bde::GetTasksPatient_UC_3_1}});
    A_21558_01.finish();

    A_19113_01.start("Register the allowed professionOIDs");
    handlerManager.onGetDo("/Task/{id}",
            std::make_unique<GetTaskHandler>(oids{
                               oid_versicherter,
                               oid_oeffentliche_apotheke,
                               oid_krankenhausapotheke}))
        .setErpUseCase(
            std::map<InnerRequestRole, bde::UseCase>{{InnerRequestRole::pharmacy, bde::GetTaskPharmacy_UC_4_8},
                                                     {InnerRequestRole::patient, bde::GetTaskPatient_UC_3_6}});
    // note: GetTaskPharmacySecretRecovery_UC_4_17 is set directly from GetTaskHandler::handleRequest
    A_19113_01.finish();

    // For POST /Task see gemSpec_FD_eRp_V1.1.1, 6.1.2
    // ... 6.1.2.1
    A_19018.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Task/$create",
            std::make_unique<CreateTaskHandler>(
                    oids{oid_arzt, oid_zahnarzt, oid_praxis_arzt, oid_zahnarztpraxis, oid_praxis_psychotherapeut, oid_krankenhaus}))
        .setErpUseCase(bde::CreateTask_UC_2_1);
    A_19018.finish();
    // ... 6.1.2.2
    A_19022.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Task/{id}/$activate",
            std::make_unique<ActivateTaskHandler>(
                    oids{oid_arzt, oid_zahnarzt, oid_praxis_arzt, oid_zahnarztpraxis, oid_praxis_psychotherapeut, oid_krankenhaus}))
        .setErpUseCase(std::map<model::PrescriptionType, bde::UseCase>{
            {model::PrescriptionType::apothekenpflichigeArzneimittel, bde::ActivateTask_UC_2_3_160},
            {model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, bde::ActivateTask_UC_2_3_200},
            {model::PrescriptionType::digitaleGesundheitsanwendungen, bde::ActivateTask_UC_2_3_162},
            {model::PrescriptionType::direkteZuweisung, bde::ActivateTask_UC_2_3_169},
            {model::PrescriptionType::direkteZuweisungPkv, bde::ActivateTask_UC_2_3_209}});
    A_19022.finish();
    // ... 6.1.2.3
    A_19166_01.start("Register the allowed professionOIDs");
    // GEMREQ-start A_19166-01
    std::unordered_set acceptEndpointOIDsWf162{oid_kostentraeger};
    std::unordered_set acceptEndpointOIDsWfOther{oid_oeffentliche_apotheke, oid_krankenhausapotheke};
    using OIDsWf = ErpRequestHandler::OIDsByWorkflow;
    using model::PrescriptionType;
    handlerManager.onPostDo("/Task/{id}/$accept",
                            std::make_unique<AcceptTaskHandler>(OIDsWf{
                                {PrescriptionType::apothekenpflichigeArzneimittel, acceptEndpointOIDsWfOther},
                                {PrescriptionType::digitaleGesundheitsanwendungen, acceptEndpointOIDsWf162},
                                {PrescriptionType::direkteZuweisung, acceptEndpointOIDsWfOther},
                                {PrescriptionType::apothekenpflichtigeArzneimittelPkv, acceptEndpointOIDsWfOther},
                                {PrescriptionType::direkteZuweisungPkv, acceptEndpointOIDsWfOther}}))
        .setErpUseCase(bde::AcceptTask_UC_4_1);
    // GEMREQ-end A_19166-01
    A_19166_01.finish();
    // ... 6.1.2.4
    A_19170_02.start("Register the allowed professionOIDs");
    // GEMREQ-start A_19170-02
    handlerManager.onPostDo("/Task/{id}/$reject",
            std::make_unique<RejectTaskHandler>(
                    oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke, oid_kostentraeger}))
        .setErpUseCase(bde::RejectTask_UC_4_2);
    // GEMREQ-end A_19170-02
    A_19170_02.finish();
    // ... 6.1.2.5
    A_19230_01.start("Register the allowed professionOIDs");
    // GEMREQ-start A_19230-01
    handlerManager.onPostDo("/Task/{id}/$close",
            std::make_unique<CloseTaskHandler>(
                    oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke, oid_kostentraeger}))
        .setErpUseCase(bde::CloseTask_UC_4_4);
    // GEMREQ-end A_19230-01
    A_19230_01.finish();
    // ... 6.1.2.6
    A_19026.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Task/{id}/$abort",
            std::make_unique<AbortTaskHandler>(
                    oids{oid_versicherter, oid_arzt, oid_zahnarzt, oid_praxis_arzt, oid_zahnarztpraxis, oid_praxis_psychotherapeut,
                                     oid_krankenhaus, oid_oeffentliche_apotheke, oid_krankenhausapotheke}))
        .setErpUseCase(
            std::map<InnerRequestRole, bde::UseCase>{{InnerRequestRole::doctor, bde::AbortTaskDoctor_UC_2_5},
                                                     {InnerRequestRole::pharmacy, bde::AbortTaskPharmacy_UC_4_3},
                                                     {InnerRequestRole::patient, bde::AbortTaskPatient_UC_3_2}});
    A_19026.finish();
    // ... 6.1.2.7 C_11574
    // GEMREQ-start A_24279
    A_24279.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Task/{id}/$dispense",
            std::make_unique<DispenseTaskHandler>(
                    oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke}))
        .setErpUseCase(bde::TaskDispense_UC_4_16);
    A_24279.finish();
    // GEMREQ-end A_24279

    // For GET /MedicationDispense see gemSpec_FD_eRp_V1.1.1, 6.2.1
    // GEMREQ-start A_19405
    A_19405_02.start("Register the allowed professionOIDs");
    const oids medicationDispenseEndpointOIDs{oid_versicherter};
    handlerManager.onGetDo("/MedicationDispense",
                 std::make_unique<GetAllMedicationDispenseHandler>(medicationDispenseEndpointOIDs))
        .setErpUseCase(bde::GetMedicationDispense_UC_3_9);
    A_19405_02.finish();
    // GEMREQ-end A_19405

    // For GET /Communication see gemSpec_FD_eRp_V1.1.1, 6.3.1
    // GEMREQ-start A_19446
    A_19446_02.start("Register the allowed professionOIDs");
    const oids communicationEndpointOIDs{oid_versicherter, oid_oeffentliche_apotheke, oid_krankenhausapotheke,
                                         oid_kostentraeger};
    const std::map<InnerRequestRole, bde::UseCase> CommunicationGetUseCase{
        {InnerRequestRole::pharmacy, bde::GetCommunicationPharmacy_UC_4_6},
        {InnerRequestRole::patient, bde::GetCommunicationPatient_UC_3_4}};
    handlerManager.onGetDo("/Communication",
            std::make_unique<CommunicationGetAllHandler>(communicationEndpointOIDs))
        .setErpUseCase(CommunicationGetUseCase);
    handlerManager.onGetDo("/Communication/{id}",
            std::make_unique<CommunicationGetByIdHandler>(communicationEndpointOIDs))
        .setErpUseCase(CommunicationGetUseCase);

    // For POST /Communication see gemSpec_FD_eRp_V1.1.1, 6.3.2
    handlerManager.onPostDo("/Communication",
            std::make_unique<CommunicationPostHandler>(communicationEndpointOIDs))
        .setErpUseCase(std::map<InnerRequestRole, bde::UseCase>{
            {InnerRequestRole::pharmacy, bde::PostCommunicationPharmacy_UC_4_7},
            {InnerRequestRole::patient, bde::PostCommunicationPatient_UC_3_3}});

    // For DELETE /Communication see gemSpec_FD_eRp_V1.1.1, 6.3.3
    handlerManager.onDeleteDo("/Communication/{id}",
            std::make_unique<CommunicationDeleteHandler>(communicationEndpointOIDs))
        .setErpUseCase(std::map<InnerRequestRole, bde::UseCase>{
            {InnerRequestRole::pharmacy, bde::DeleteCommunicationPharmacy_UC_4_9},
            {InnerRequestRole::patient, bde::DeleteCommunicationPatient_UC_3_8}});
    A_19446_02.finish();
    // GEMREQ-end A_19446


    A_19395.start("Register the allowed professionOIDs");
    const oids auditEventHandlerOIDs{oid_versicherter};
    // For GET /AuditEvent see gemSpec_FD_eRp_V1.1.1, 6.4.1
    handlerManager.onGetDo("/AuditEvent",
            std::make_unique<GetAllAuditEventsHandler>(auditEventHandlerOIDs))
        .setErpUseCase(bde::GetAuditEvent_UC_3_5);
    handlerManager.onGetDo("/AuditEvent/{id}",
            std::make_unique<GetAuditEventHandler>(auditEventHandlerOIDs))
        .setErpUseCase(bde::GetAuditEvent_UC_3_5);
    A_19395.finish();

    // For GET /Device see gemSpec_FD_eRp_V1.1.1, 6.5
    A_20744.start("add GET /Device endpoint");
    handlerManager.onGetDo("/Device",
            std::make_unique<DeviceHandler>())
        .setErpUseCase(bde::GetDevice_UC_1_1);
    A_20744.finish();

    // For GET /Device see gemSpec_FD_eRp_V1.1.1, 6
    A_20171.start("add GET /metadata endpoint");
    handlerManager.onGetDo("/metadata",
            std::make_unique<MetaDataHandler>())
        .setErpUseCase(bde::GetMetadata_UC_1_2);
    A_20171.finish();

    // For POST /Subscription
    // GEMREQ-start A_22362
    A_22362_01.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Subscription",
                            std::make_unique<SubscriptionPostHandler>(
                                oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke, oid_kostentraeger}))
        .setErpUseCase(bde::PostSubscription_UC_4_14);
    A_22362_01.finish();
    // GEMREQ-end A_22362

    // PKV endpoints:

    // Resource ChargeItem (gemF_eRp_PKV_V1.0.1, 6.1.4)

    A_22112.start("If called without specifying an <id>, issue error code 405 to prevent "
                  "deletion of multiple resources in one request via a single request");
    // GEMREQ-start A_22113#regop
    A_22113.start("Register the allowed professionOIDs");
    handlerManager.onDeleteDo("/ChargeItem/{id}", std::make_unique<ChargeItemDeleteHandler>(oids{oid_versicherter}))
        .setErpUseCase(bde::DeleteChargeItem_UC_3_11);
    A_22113.finish();
    // GEMREQ-end A_22113#regop
    A_22112.finish();

    A_22879.start("If called without specifying an <id>, issue error code 405 to prevent "
                  "changing multiple resources in one request via a single request");
    // GEMREQ-start A_22875#regop
    A_22875.start("Only 'oid_versicherter' users are allowed to call the operation");
    handlerManager.onPatchDo("/ChargeItem/{id}", std::make_unique<ChargeItemPatchHandler>(oids{oid_versicherter}))
        .setErpUseCase(bde::PatchChargeItem_UC_3_12);
    A_22875.finish();
    // GEMREQ-end A_22875#regop
    A_22879.finish();

    // GEMREQ-start A_22118#regop
    A_22118.start("Register the allowed professionOIDs");
    handlerManager.onGetDo("/ChargeItem", std::make_unique<ChargeItemGetAllHandler>(oids{oid_versicherter}))
        .setErpUseCase(bde::GetChargeItems_UC_3_10);
    A_22118.finish();
    // GEMREQ-end A_22118#regop

    // GEMREQ-start A_22124#regop
    A_22124.start("Register the allowed professionOIDs");
    handlerManager.onGetDo("/ChargeItem/{id}",
                           std::make_unique<ChargeItemGetByIdHandler>(
                               oids{oid_versicherter, oid_oeffentliche_apotheke, oid_krankenhausapotheke}))
        .setErpUseCase(
            std::map<InnerRequestRole, bde::UseCase>{{InnerRequestRole::pharmacy, bde::GetChargeItemPharmacy_UC_4_10},
                                                     {InnerRequestRole::patient, bde::GetChargeItemPatient_UC_3_7}});
    A_22124.finish();
    // GEMREQ-end A_22124#regop

    // GEMREQ-start A_22129#regop
    A_22129.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/ChargeItem", std::make_unique<ChargeItemPostHandler>(
                                                oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke}))
        .setErpUseCase(bde::PostChargeItem_UC_4_11);
    A_22129.finish();
    // GEMREQ-end A_22129#regop

    // GEMREQ-start A_22144#regop
    A_22144.start("Register the allowed professionOIDs");
    handlerManager.onPutDo("/ChargeItem/{id}", std::make_unique<ChargeItemPutHandler>(
                                                   oids{oid_oeffentliche_apotheke, oid_krankenhausapotheke}))
        .setErpUseCase(bde::PutChargeItem_UC_4_13);
    A_22144.finish();
    // GEMREQ-end A_22144#regop

    // Resource Consent (gemF_eRp_PKV_V1.0.1, 6.1.5)

    // GEMREQ-start consentoids
    const oids consentOIDs{oid_versicherter};
    // GEMREQ-end consentoids
    // GEMREQ-start A_22155#regop
    A_22155.start("Register the allowed professionOIDs");
    handlerManager.onDeleteDo("/Consent", std::make_unique<ConsentDeleteHandler>(consentOIDs))
        .setErpUseCase(bde::DeleteConsent_UC_3_15);
    A_22155.finish();
    // GEMREQ-end A_22155#regop

    // GEMREQ-start A_22159#regop
    A_22159.start("Register the allowed professionOIDs");
    handlerManager.onGetDo("/Consent", std::make_unique<ConsentGetHandler>(consentOIDs))
        .setErpUseCase(bde::GetConsent_UC_3_13);
    A_22159.finish();
    // GEMREQ-end A_22159#regop

    // GEMREQ-start A_22161#regop
    A_22161.start("Register the allowed professionOIDs");
    handlerManager.onPostDo("/Consent", std::make_unique<ConsentPostHandler>(consentOIDs))
        .setErpUseCase(bde::PostConsent_UC_3_14);
    A_22161.finish();
    // GEMREQ-end A_22161#regop

    if (Configuration::instance().getBoolValue(ConfigurationKey::FEATURE_EU))
    {
        // GEMREQ-start A_27088#granteuoids
        const oids euAccessPermissionOIDs{oid_versicherter};
        A_27088.start("Register the allowed professionOIDs");
        handlerManager.onPostDo("/$grant-eu-access-permission",
                                std::make_unique<eu_access_permission::GrantHandler>(euAccessPermissionOIDs))
            .setErpUseCase(bde::GrantEuAuthorization_UC_3_16);
        A_27088.finish();
        // GEMREQ-end A_27088#granteuoids

        // GEMREQ-start A_27086
        A_27086.start("Register the allowed professionOIDs");
        handlerManager.onGetDo("/$read-eu-access-permission",
                               std::make_unique<eu_access_permission::ReadHandler>(euAccessPermissionOIDs))
            .setErpUseCase(bde::ReadEuAuthorization_UC_3_18);
        A_27086.finish();
        // GEMREQ-end A_27086

        // GEMREQ-start A_27084
        A_27084.start("Register the allowed professionOIDs");
        handlerManager.onDeleteDo("/$revoke-eu-access-permission",
                                  std::make_unique<eu_access_permission::RevokeHandler>(euAccessPermissionOIDs))
            .setErpUseCase(bde::RevokeEuAuthorization_UC_3_17);
        A_27084.finish();
        // GEMREQ-end A_27084

        A_27548.start("If called without specifying an <id>, issue error code 405 to prevent "
                      "changing multiple resources in one request via a single request");
        A_27549.start("Only 'oid_versicherter' users are allowed to call the operation");
        handlerManager.onPatchDo("/Task/{id}", std::make_unique<eu::PatchTaskHandler>(euAccessPermissionOIDs))
            .setErpUseCase(bde::PatchTask_UC_PATCH_TASK);
        A_27549.finish();
        A_27548.finish();

        // GEMREQ-start A_27059
        A_27059.start("Register the allowed professionOIDs");
        handlerManager.onPostDo("/$get-eu-prescriptions",
                               std::make_unique<eu::PostGetPrescriptionsHandler>(oids{oid_ncpeh}))
            .setErpUseCase(bde::NoUseCase{});// set directly in handleRequest.
        A_27059.finish();
        // GEMREQ-end A_27059

        // GEMREQ-start A_27068
        A_27068.start("Register the allowed professionOIDs");
        handlerManager.onPostDo("/Task/{id}/$eu-close", std::make_unique<eu::EuCloseTaskHandler>(oids{oid_ncpeh}))
            .setErpUseCase(bde::EuClose_UC_4_22);
        A_27068.finish();
        // GEMREQ-end A_27068
    }
}
}
