/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX

#include "erp/model/Resource.hxx"

namespace model
{

class Reference : public Resource<Reference>
{
public:
    [[nodiscard]] std::optional<std::string_view> identifierUse() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingCode() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierValue() const;
private:
    friend Resource<Reference>;
    explicit Reference(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_REFERENCE_HXX
