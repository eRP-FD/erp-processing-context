/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/KbvPracticeSupply.hxx"

namespace model
{
KbvPracticeSupply::KbvPracticeSupply(NumberAsStringParserDocument&& document)
    : Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>(std::move(document))
{
}
}
