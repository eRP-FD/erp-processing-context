/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX

#include "shared/model/Resource.hxx"

namespace model
{


// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvPracticeSupply : public Resource<KbvPracticeSupply>
{
public:
    static constexpr auto resourceTypeName = "SupplyRequest";
private:
    friend Resource<KbvPracticeSupply>;
    explicit KbvPracticeSupply(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvPracticeSupply>;
}
#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX
