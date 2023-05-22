/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvPracticeSupply.hxx"

namespace model
{
KbvPracticeSupply::KbvPracticeSupply(NumberAsStringParserDocument&& document)
    : Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>(std::move(document))
{
}
}
