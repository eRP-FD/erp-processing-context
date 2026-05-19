// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include <string>

namespace bde
{

struct UseCase {
    enum UC
    {
        UC_1_1,
        UC_1_2,

        UC_2_1,
        UC_2_3,
        UC_2_3_169,
        UC_2_3_162,
        UC_2_3_166,
        UC_2_3_200,
        UC_2_3_209,
        UC_2_5,

        UC_3_1,
        UC_3_2,
        UC_3_3,
        UC_3_4,
        UC_3_5,
        UC_3_6,
        UC_3_7,
        UC_3_8,
        UC_3_9,
        UC_3_10,
        UC_3_11,
        UC_3_12,
        UC_3_13,
        UC_3_14,
        UC_3_15,

        UC_3_16,
        UC_3_17,
        UC_3_18,
        UC_3_19,

        UC_4_1,
        UC_4_2,
        UC_4_3,
        UC_4_4,
        UC_4_6,
        UC_4_7,
        UC_4_8,
        UC_4_9,
        UC_4_10,
        UC_4_11,
        UC_4_12,
        UC_4_13,
        UC_4_14,
        UC_4_15,
        UC_4_16,
        UC_4_17,
        UC_4_19,
        UC_4_20,
        UC_4_21,
        UC_4_22
    };

    UC fdOperation;
    bool operator==(const UseCase&) const = default;
};
std::string to_string(const UseCase& uc);

static constexpr UseCase GetDevice_UC_1_1{.fdOperation = UseCase::UC_1_1};
static constexpr UseCase GetMetadata_UC_1_2{.fdOperation = UseCase::UC_1_2};

static constexpr UseCase CreateTask_UC_2_1{.fdOperation = UseCase::UC_2_1};
static constexpr UseCase ActivateTask_UC_2_3_160{.fdOperation = UseCase::UC_2_3};
static constexpr UseCase ActivateTask_UC_2_3_169{.fdOperation = UseCase::UC_2_3_169};
static constexpr UseCase ActivateTask_UC_2_3_162{.fdOperation = UseCase::UC_2_3_162};
static constexpr UseCase ActivateTask_UC_2_3_166{.fdOperation = UseCase::UC_2_3_166};
static constexpr UseCase ActivateTask_UC_2_3_200{.fdOperation = UseCase::UC_2_3_200};
static constexpr UseCase ActivateTask_UC_2_3_209{.fdOperation = UseCase::UC_2_3_209};
static constexpr UseCase AbortTaskDoctor_UC_2_5{.fdOperation = UseCase::UC_2_5};
static constexpr UseCase GetTasksPatient_UC_3_1{.fdOperation = UseCase::UC_3_1};
static constexpr UseCase AbortTaskPatient_UC_3_2{.fdOperation = UseCase::UC_3_2};
static constexpr UseCase PostCommunicationPatient_UC_3_3{.fdOperation = UseCase::UC_3_3};
static constexpr UseCase GetCommunicationPatient_UC_3_4{.fdOperation = UseCase::UC_3_4};
static constexpr UseCase GetAuditEvent_UC_3_5{.fdOperation = UseCase::UC_3_5};
static constexpr UseCase GetTaskPatient_UC_3_6{.fdOperation = UseCase::UC_3_6};
static constexpr UseCase GetChargeItemPatient_UC_3_7{.fdOperation = UseCase::UC_3_7};
static constexpr UseCase DeleteCommunicationPatient_UC_3_8{.fdOperation = UseCase::UC_3_8};
static constexpr UseCase GetMedicationDispense_UC_3_9{.fdOperation = UseCase::UC_3_9};
static constexpr UseCase GetChargeItems_UC_3_10{.fdOperation = UseCase::UC_3_10};
static constexpr UseCase DeleteChargeItem_UC_3_11{.fdOperation = UseCase::UC_3_11};
static constexpr UseCase PatchChargeItem_UC_3_12{.fdOperation = UseCase::UC_3_12};
static constexpr UseCase GetConsent_UC_3_13{.fdOperation = UseCase::UC_3_13};
static constexpr UseCase PostConsent_UC_3_14{.fdOperation = UseCase::UC_3_14};
static constexpr UseCase DeleteConsent_UC_3_15{.fdOperation = UseCase::UC_3_15};

static constexpr UseCase GrantEuAuthorization_UC_3_16{.fdOperation = UseCase::UC_3_16};
static constexpr UseCase RevokeEuAuthorization_UC_3_17{.fdOperation = UseCase::UC_3_17};
static constexpr UseCase ReadEuAuthorization_UC_3_18{.fdOperation = UseCase::UC_3_18};
static constexpr UseCase PatchTask_UC_3_19{.fdOperation = UseCase::UC_3_19};

static constexpr UseCase AcceptTask_UC_4_1{.fdOperation = UseCase::UC_4_1};
static constexpr UseCase RejectTask_UC_4_2{.fdOperation = UseCase::UC_4_2};
static constexpr UseCase AbortTaskPharmacy_UC_4_3{.fdOperation = UseCase::UC_4_3};
static constexpr UseCase CloseTask_UC_4_4{.fdOperation = UseCase::UC_4_4};
static constexpr UseCase GetCommunicationPharmacy_UC_4_6{.fdOperation = UseCase::UC_4_6};
static constexpr UseCase PostCommunicationPharmacy_UC_4_7{.fdOperation = UseCase::UC_4_7};
static constexpr UseCase GetTaskPharmacy_UC_4_8{.fdOperation = UseCase::UC_4_8};
static constexpr UseCase DeleteCommunicationPharmacy_UC_4_9{.fdOperation = UseCase::UC_4_9};
static constexpr UseCase GetChargeItemPharmacy_UC_4_10{.fdOperation = UseCase::UC_4_10};
static constexpr UseCase PostChargeItem_UC_4_11{.fdOperation = UseCase::UC_4_11};
static constexpr UseCase GetTasksPharmacy_UC_4_12{.fdOperation = UseCase::UC_4_12};
static constexpr UseCase PutChargeItem_UC_4_13{.fdOperation = UseCase::UC_4_13};
static constexpr UseCase PostSubscription_UC_4_14{.fdOperation = UseCase::UC_4_14};
static constexpr UseCase GetTasksPoPP_UC_4_15{.fdOperation = UseCase::UC_4_15};
static constexpr UseCase TaskDispense_UC_4_16{.fdOperation = UseCase::UC_4_16};
static constexpr UseCase GetTaskPharmacySecretRecovery_UC_4_17{.fdOperation = UseCase::UC_4_17};

static constexpr UseCase GetDemographics_UC_4_19{.fdOperation = UseCase::UC_4_19};
static constexpr UseCase GetPrescriptionsList_UC_4_20{.fdOperation = UseCase::UC_4_20};
static constexpr UseCase GetPrescriptionsRetrieval_UC_4_21{.fdOperation = UseCase::UC_4_21};
static constexpr UseCase EuClose_UC_4_22{.fdOperation = UseCase::UC_4_22};

struct NoUseCase {
};
std::string to_string(const NoUseCase& uc);
}