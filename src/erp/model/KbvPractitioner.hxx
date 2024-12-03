/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVPRACTITIONER_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVPRACTITIONER_HXX_

#include "shared/model/Resource.hxx"
#include "erp/model/Lanr.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvPractitioner : public Resource<KbvPractitioner>
{
public:
    static constexpr auto resourceTypeName = "Practitioner";
    [[nodiscard]] std::string_view qualificationTypeCode() const;
    [[nodiscard]] std::optional<std::string_view> nameFamily() const;
    [[nodiscard]] std::optional<UnspecifiedResource> name_family() const;
    [[nodiscard]] std::optional<std::string_view> namePrefix() const;
    [[nodiscard]] std::optional<UnspecifiedResource> name_prefix() const;
    [[nodiscard]] std::optional<std::string_view> qualificationText(size_t idx) const;
    [[nodiscard]] std::optional<std::string_view> qualificationCodeCodingSystem(size_t idx) const;
    [[nodiscard]] std::optional<std::string_view> qualificationCodeCodingCode(size_t idx) const;
    [[nodiscard]] std::optional<Lanr> anr() const;
    [[nodiscard]] std::optional<Lanr> zanr() const;

private:
    friend Resource<KbvPractitioner>;
    explicit KbvPractitioner(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<class KbvPractitioner>;
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVPRACTITIONER_HXX_
