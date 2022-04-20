/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_HXX
#define ERP_PROCESSING_CONTEXT_TASK_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/PrescriptionType.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>

#include <optional>

namespace model
{

enum class KbvStatusKennzeichen;

class Task : public Resource<Task>
{
public:
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
    explicit Task(model::PrescriptionType prescriptionType, const std::optional<std::string_view>& accessCode);

    // For creation from database entry
    explicit Task(const PrescriptionId& id, PrescriptionType prescriptionType,
                  Timestamp lastModified, Timestamp authoredOn, Status status);

    [[nodiscard]] Status status() const;
    [[nodiscard]] std::optional<std::string_view> kvnr() const;
    [[nodiscard]] PrescriptionId prescriptionId() const;
    [[nodiscard]] Timestamp authoredOn() const;
    [[nodiscard]] PrescriptionType type() const;
    [[nodiscard]] Timestamp lastModifiedDate() const;
    [[nodiscard]] std::string_view accessCode() const;
    [[nodiscard]] Timestamp expiryDate() const;
    [[nodiscard]] Timestamp acceptDate() const;
    [[nodiscard]] std::optional<std::string_view> secret() const;
    [[nodiscard]] std::optional<std::string_view> healthCarePrescriptionUuid() const;
    [[nodiscard]] std::optional<std::string_view> patientConfirmationUuid() const;
    [[nodiscard]] std::optional<std::string_view> receiptUuid() const;

    void setPrescriptionId (const model::PrescriptionId& prescriptionId);
    void setStatus(Status newStatus);
    void setKvnr(std::string_view kvnr);
    // The UUID are derived from the prescription ID
    void setHealthCarePrescriptionUuid();
    void setPatientConfirmationUuid();
    void setReceiptUuid();
    void setExpiryDate(const Timestamp& expiryDate);
    void setAcceptDate(const Timestamp& acceptDate);
    void setAcceptDate(const Timestamp& baseTime, const KbvStatusKennzeichen& legalBasisCode,
                       int entlassRezeptValidityWorkingDays);
    void setSecret(std::string_view secret);
    void setAccessCode(std::string_view accessCode);

    // functions for the deletion of personal data:
    void deleteKvnr();
    void deleteInput();
    void deleteOutput();
    void deleteAccessCode();
    void deleteSecret();

    void updateLastUpdate(const Timestamp& timestamp = Timestamp::now());

private:
    friend Resource<Task>;
    explicit Task(NumberAsStringParserDocument&& document);

    [[nodiscard]] std::optional<std::string_view> uuidFromArray(const rapidjson::Pointer& array, std::string_view code) const;
    void addUuidToArray(const rapidjson::Pointer& array, std::string_view code, std::string_view uuid);
    void dateToExtensionArray(std::string_view url, const Timestamp& expiryDate);
    [[nodiscard]] Timestamp dateFromExtensionArray(std::string_view url) const;

    // based on task status set the expected UUIDs in the input/output arrays.
    void setInputOutputUuids();
    void setAccepDateDependentPrescriptionType(const Timestamp& baseTime);
};

}

#endif //ERP_PROCESSING_CONTEXT_TASK_HXX
