/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVCOMPOSITION_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVCOMPOSITION_HXX

#include "erp/model/Resource.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvComposition : public Resource<KbvComposition, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "Composition";
    [[nodiscard]] std::optional<std::string_view> sectionEntry(const std::string_view& sectionCode) const;
    [[nodiscard]] std::optional<std::string_view> subject() const;
    [[nodiscard]] std::optional<std::string_view> authorType(size_t idx) const;
    [[nodiscard]] std::optional<std::string_view> authorReference(size_t idx) const;
    [[nodiscard]] std::optional<std::string_view> authorIdentifierSystem(size_t idx) const;
    [[nodiscard]] std::optional<std::string_view> authorIdentifierValue(size_t idx) const;

private:
    friend Resource<KbvComposition, ResourceVersion::KbvItaErp>;
    explicit KbvComposition(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVCOMPOSITION_HXX
