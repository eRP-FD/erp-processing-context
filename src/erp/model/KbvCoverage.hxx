/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVCOVERAGE_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVCOVERAGE_HXX_

#include "erp/model/Resource.hxx"
#include "erp/model/Iknr.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvCoverage : public Resource<KbvCoverage, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "Coverage";
    [[nodiscard]] std::string_view typeCodingCode() const;
    [[nodiscard]] bool hasPayorIdentifierExtension(const std::string_view& url) const;
    [[nodiscard]] std::optional<model::Iknr> payorIknr() const;
    [[nodiscard]] std::optional<model::Iknr> alternativeId() const;
    [[nodiscard]] std::optional<std::string_view> periodEnd() const;
    [[nodiscard]] const rapidjson::Value* payorIdentifier() const;

private:
    friend Resource<KbvCoverage, ResourceVersion::KbvItaErp>;
    explicit KbvCoverage(NumberAsStringParserDocument&& jsonTree);
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVCOVERAGE_HXX_
