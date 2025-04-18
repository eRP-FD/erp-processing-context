/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONBASE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONBASE_HXX

#include "shared/model/Resource.hxx"

namespace model
{

template<class TDerivedMedication>
// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationBase : public Resource<TDerivedMedication>
{
public:
    explicit MedicationBase(NumberAsStringParserDocument&& document);
    static constexpr auto resourceTypeName = "Medication";

private:
    friend Resource<TDerivedMedication>;
    using Resource<TDerivedMedication>::Resource;
};


template<class TDerivedMedication>
MedicationBase<TDerivedMedication>::MedicationBase(NumberAsStringParserDocument&& document)
    : Resource<TDerivedMedication>(std::move(document))
{
}

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONBASE_HXX
