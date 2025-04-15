/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvPracticeSupply.hxx"

namespace model
{
KbvPracticeSupply::KbvPracticeSupply(NumberAsStringParserDocument&& document)
    : Resource<KbvPracticeSupply>(std::move(document))
{
}
}
