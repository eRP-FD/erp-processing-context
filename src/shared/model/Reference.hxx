/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX

#include "shared/model/Resource.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class Reference : public Resource<Reference>
{
public:
    static constexpr auto resourceTypeName = "Reference";

    [[nodiscard]] std::optional<std::string_view> identifierUse() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingCode() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierValue() const;
private:
    friend Resource<Reference>;
    explicit Reference(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Reference>;
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX
