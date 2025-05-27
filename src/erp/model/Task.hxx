/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_HXX
#define ERP_PROCESSING_CONTEXT_TASK_HXX

#include "shared/model/PrescriptionId.hxx"
#include "shared/model/PrescriptionType.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/model/Kvnr.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <optional>

namespace model
{

enum class KbvStatusKennzeichen;

// NOLINTNEXTLINE(bugprone-exception-escape)
class Task : public Resource<Task>
{
public:
    static constexpr auto resourceTypeName = "Task";
    static constexpr auto profileType = ProfileType::GEM_ERP_PR_Task;

    // we support a subset of https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/80166
    // as defined in table TAB_SYSLERP_006 of gemSysL_eRp
    enum class Status : int8_t
    {
        draft = 0, // initialisiert
        ready = 1, // offen
        inprogress = 2, // in Abgabe (gesperrt)
        completed = 3, // quittiert
        cancelled = 4 // gelöscht
    };
    static const std::unordered_map<Status, std::string_view> StatusNames;
    static const std::unordered_map<std::string_view, Status> StatusNamesReverse;
    static int8_t toNumericalStatus(Status status);
    static Status fromNumericalStatus(int8_t status);

    // For creation of a new task
    explicit Task(model::PrescriptionType prescriptionType, const std::optional<std::string_view>& accessCode,
                  Timestamp lastStatusChange = Timestamp::now());

    // For creation from database entry
    explicit Task(const PrescriptionId& id, PrescriptionType prescriptionType, model::Timestamp lastModified,
                  model::Timestamp authoredOn, Status status, Timestamp lastStatusChange);

    [[nodiscard]] Status status() const;
    [[nodiscard]] std::optional<Kvnr> kvnr() const;
    [[nodiscard]] PrescriptionId prescriptionId() const;
    [[nodiscard]] model::Timestamp authoredOn() const;
    [[nodiscard]] PrescriptionType type() const;
    [[nodiscard]] model::Timestamp lastModifiedDate() const;
    [[nodiscard]] std::string_view accessCode() const;
    [[nodiscard]] model::Timestamp expiryDate() const;
    [[nodiscard]] model::Timestamp acceptDate() const;
    [[nodiscard]] std::optional<model::Timestamp> lastMedicationDispense() const;
    [[nodiscard]] std::optional<std::string_view> secret() const;
    [[nodiscard]] std::optional<std::string_view> healthCarePrescriptionUuid() const;
    [[nodiscard]] std::optional<std::string_view> patientConfirmationUuid() const;
    [[nodiscard]] std::optional<std::string_view> receiptUuid() const;
    [[nodiscard]] std::optional<std::string_view> owner() const;
    [[nodiscard]] const Timestamp& lastStatusChangeDate() const;

    void setPrescriptionId (const model::PrescriptionId& prescriptionId);
    void setStatus(Status newStatus);
    void setKvnr(const Kvnr& kvnr);
    // The UUID are derived from the prescription ID
    void setHealthCarePrescriptionUuid();
    void setPatientConfirmationUuid();
    void setReceiptUuid();
    void setExpiryDate(const model::Timestamp& expiryDate);
    void setAcceptDate(const model::Timestamp& acceptDate);
    void setAcceptDate(const model::Timestamp& baseTime, const std::optional<KbvStatusKennzeichen>& legalBasisCode,
                       int entlassRezeptValidityWorkingDays);
    void updateLastMedicationDispense(
        const std::optional<model::Timestamp>& lastMedicationDispense = std::make_optional(model::Timestamp::now()));
    void removeLastMedicationDispenseConditional();
    void setSecret(std::string_view secret);
    void setAccessCode(std::string_view accessCode);
    void setOwner(std::string_view owner);

    void deleteAccessCode();
    void deleteSecret();
    void deleteOwner();
    void deleteLastMedicationDispense();

    void updateLastUpdate(const model::Timestamp& timestamp = model::Timestamp::now());

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend Resource<Task>;
    explicit Task(NumberAsStringParserDocument&& jsonTree);

    [[nodiscard]] std::optional<std::string_view> uuidFromArray(const rapidjson::Pointer& array, std::string_view code) const;
    void addUuidToArray(const rapidjson::Pointer& array, std::string_view code, std::string_view uuid);
    void dateToExtensionArray(std::string_view url, const model::Timestamp& expiryDate);
    void instantToExtensionArray(std::string_view url, const std::optional<model::Timestamp>& expiryDate);
    [[nodiscard]] model::Timestamp dateFromExtensionArray(std::string_view url) const;

    void setAcceptDateDependentPrescriptionType(const model::Timestamp& baseTime);
    void setStatus(Status newStatus, model::Timestamp lastStatusChange);

    model::Timestamp mLastStatusChange;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Task>;
}

#endif //ERP_PROCESSING_CONTEXT_TASK_HXX
