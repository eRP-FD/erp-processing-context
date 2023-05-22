/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX

#include "erp/model/Resource.hxx"

namespace model
{


// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvPracticeSupply : public Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "SupplyRequest";
private:
    friend Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>;
    explicit KbvPracticeSupply(NumberAsStringParserDocument&& document);
};

}
#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVPRACTICESUPPLY_HXX
