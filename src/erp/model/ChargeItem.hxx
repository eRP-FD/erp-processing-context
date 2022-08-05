/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX

#include "erp/model/extensions/ChargeItemMarkingFlag.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "fhirtools/model/Timestamp.hxx"

#include <optional>

namespace model
{

class ChargeItem : public Resource<ChargeItem>
{
public:
    static constexpr auto resourceTypeName = "ChargeItem";

    enum class SupportingInfoType : int8_t
    {
        prescriptionItem,
        dispenseItem,
        receipt
    };
    static const std::unordered_map<SupportingInfoType,
                                    std::pair<std::string_view, std::string_view>> SupportingInfoTypeNames;

    ChargeItem();

    [[nodiscard]] ::std::optional<PrescriptionId> id() const;
    [[nodiscard]] ::std::optional<PrescriptionId> prescriptionId() const;
    [[nodiscard]] ::std::optional<::std::string_view> subjectKvnr() const;
    [[nodiscard]] ::std::optional<::std::string_view> entererTelematikId() const;
    [[nodiscard]] ::std::optional<fhirtools::Timestamp> enteredDate() const;
    [[nodiscard]] std::optional<std::string_view> supportingInfoReference(SupportingInfoType supportingInfoType) const;

    [[nodiscard]] bool isMarked() const;
    [[nodiscard]] std::optional<model::ChargeItemMarkingFlag> markingFlag() const;

    [[nodiscard]] std::optional<std::string_view> accessCode() const;

    [[nodiscard]] std::optional<model::Binary> containedBinary() const;

    // Please note that the prescriptionId of the task is also used as the id of the ChargeItem resource.
    void setId(const model::PrescriptionId& prescriptionId);
    void setPrescriptionId(const model::PrescriptionId& prescriptionId);
    void setSubjectKvnr(const std::string_view& kvnr);
    void setEntererTelematikId(const std::string_view& telematicId);
    void setEnteredDate(const fhirtools::Timestamp& entered);
    void setMarkingFlag(const ::model::Extension& markingFlag);
    void setAccessCode(const std::string_view& accessCode);
    template<class ResourceType>
    void setSupportingInfoReference(SupportingInfoType supportingInfoType, const ResourceType& resource);

    void deleteAccessCode();
    void deleteSupportingInfoReference(SupportingInfoType supportingInfoType);
    void deleteContainedBinary();
    void deleteMarkingFlag();

private:
    friend Resource<ChargeItem>;
    explicit ChargeItem(NumberAsStringParserDocument&& jsonTree);
};

struct ChargeInformation {
    ::model::ChargeItem chargeItem;
    ::std::optional<::model::Binary> prescription;
    ::std::optional<::model::Bundle> unsignedPrescription;
    ::model::Binary dispenseItem;
    ::model::Bundle unsignedDispenseItem;
    ::std::optional<::model::Bundle> receipt;
};

extern template void ChargeItem::setSupportingInfoReference<::model::Bundle>(SupportingInfoType supportingInfoType,
                                                                             const ::model::Bundle& resource);
extern template void ChargeItem::setSupportingInfoReference<::std::string>(SupportingInfoType supportingInfoType,
                                                                           const ::std::string& resource);
extern template void ChargeItem::setSupportingInfoReference<::std::string_view>(SupportingInfoType supportingInfoType,
                                                                                const ::std::string_view& resource);

}

#endif// ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX
