/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationDummy.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{

KbvMedicationDummy::KbvMedicationDummy(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationDummy, ResourceVersion::KbvItaErp>(std::move(document))
{
}

}// namespace model
