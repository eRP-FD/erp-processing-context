/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX

#include "erp/model/KbvMedicationBase.hxx"

namespace model
{

class KbvMedicationPzn : public KbvMedicationBase<KbvMedicationPzn, ResourceVersion::KbvItaErp>
{
public:
    [[nodiscard]] const std::optional<std::string_view> amountNumeratorValueAsString() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorSystem() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorCode() const;
private:
    friend Resource<KbvMedicationPzn, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationPzn(NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
