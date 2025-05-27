/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX

#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class ChargeItem : public Resource<ChargeItem>
{
public:
    static constexpr auto resourceTypeName = "ChargeItem";
    static constexpr auto profileType = ProfileType::GEM_ERPCHRG_PR_ChargeItem;

    enum class SupportingInfoType : int8_t
    {
        prescriptionItemBundle,
        dispenseItemBundle,
        dispenseItemBinary,
        receiptBundle
    };
    static const std::unordered_map<SupportingInfoType, std::string_view> SupportingInfoDisplayNames;

    ChargeItem();

    [[nodiscard]] ::std::optional<PrescriptionId> id() const;
    [[nodiscard]] ::std::optional<PrescriptionId> prescriptionId() const;
    [[nodiscard]] ::std::optional<Kvnr> subjectKvnr() const;
    [[nodiscard]] ::std::optional<::std::string_view> entererTelematikId() const;
    [[nodiscard]] ::std::optional<model::Timestamp> enteredDate() const;
    [[nodiscard]] std::optional<std::string_view> supportingInfoReference(SupportingInfoType supportingInfoType) const;

    [[nodiscard]] bool isMarked() const;
    [[nodiscard]] std::optional<model::ChargeItemMarkingFlags> markingFlags() const;

    [[nodiscard]] std::optional<std::string_view> accessCode() const;

    /**
     * @return std::optional<model::Binary> Returns the binary data if present.
     */
    [[nodiscard]] std::optional<model::Binary> containedBinary() const;

    // Please note that the prescriptionId of the task is also used as the id of the ChargeItem resource.
    void setId(const model::PrescriptionId& prescriptionId);
    void setPrescriptionId(const model::PrescriptionId& prescriptionId);
    void setSubjectKvnr(std::string_view kvnr);
    void setSubjectKvnr(const Kvnr& kvnr);
    void setEntererTelematikId(std::string_view telematicId);
    void setEnteredDate(const model::Timestamp& entered);
    void setMarkingFlags(const ChargeItemMarkingFlags& markingFlags);
    void setAccessCode(std::string_view accessCode);
    // The references are derived from the prescription ID using PrescriptionId::deriveUuid
    void setSupportingInfoReference(SupportingInfoType supportingInfoType);

    void deleteAccessCode();
    void deleteSupportingInfoReference(SupportingInfoType supportingInfoType);

    /**
     * Remove the binary data from the resource
     * @param withSupportingReference Also remove the reference in supportingInformation to the binary.
     */
    void deleteContainedBinary(bool withSupportingReference = true);
    void deleteMarkingFlag();

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

    void prepare() override;

private:
    friend Resource<ChargeItem>;
    explicit ChargeItem(NumberAsStringParserDocument&& jsonTree);
    std::string supportingInfoTypeToReference(SupportingInfoType supportingInfoType) const;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
struct ChargeInformation {
    ::model::ChargeItem chargeItem;
    ::std::optional<::model::Binary> prescription;
    ::std::optional<::model::Bundle> unsignedPrescription;
    ::std::optional<::model::Binary> dispenseItem;
    ::std::optional<::model::AbgabedatenPkvBundle> unsignedDispenseItem;
    ::std::optional<::model::Bundle> receipt;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<ChargeItem>;
}

#endif// ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX
