/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CHARGEITEM_HXX

#include "erp/model/extensions/ChargeItemMarkingFlag.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Timestamp.hxx"

#include <optional>

namespace model
{

class ChargeItem : public Resource<ChargeItem>
{
public:
    enum class SupportingInfoType : int8_t
    {
        prescriptionItem,
        dispenseItem,
        receipt
    };
    static const std::unordered_map<SupportingInfoType,
                                    std::pair<std::string_view, std::string_view>> SupportingInfoTypeNames;

    [[nodiscard]] model::PrescriptionId id() const;
    [[nodiscard]] model::PrescriptionId prescriptionId() const;
    [[nodiscard]] std::string_view subjectKvnr() const;
    [[nodiscard]] std::string_view entererTelematikId() const;
    [[nodiscard]] model::Timestamp enteredDate() const;
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
    void setEnteredDate(const model::Timestamp& entered);
    void setMarkingFlag(const model::ChargeItemMarkingFlag& markingFlag);
    void setAccessCode(const std::string_view& accessCode);

    void setSupportingInfoReference(SupportingInfoType supportingInfoType, const std::string_view& reference);
    void deleteSupportingInfoElement(SupportingInfoType supportingInfoType);
    void deleteContainedBinary();
    void deleteMarkingFlag();

private:
    friend Resource<ChargeItem>;
    explicit ChargeItem(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
