/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H

#include "erp/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVMultiplePrescription : public model::Extension
{
public:
    class Kennzeichen;
    class Nummerierung;
    class Zeitraum;

    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription";
    [[nodiscard]] bool isMultiplePrescription() const;
    [[nodiscard]] std::optional<int> numerator() const;
    [[nodiscard]] std::optional<int> denominator() const;
    [[nodiscard]] std::optional<model::Timestamp> startDate() const;
    [[nodiscard]] std::optional<model::Timestamp> endDate() const;

    [[nodiscard]] std::optional<model::Timestamp> startDateTime() const;
    [[nodiscard]] std::optional<model::Timestamp> endDateTime() const;

    friend std::optional<KBVMultiplePrescription> ResourceBase::getExtension<KBVMultiplePrescription>(const std::string_view&) const;
};

class KBVMultiplePrescription::Kennzeichen : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "Kennzeichen";

    friend std::optional<Kennzeichen> ResourceBase::getExtension<Kennzeichen>(const std::string_view&) const;
};

class KBVMultiplePrescription::Nummerierung : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "Nummerierung";

    friend std::optional<Nummerierung> ResourceBase::getExtension<Nummerierung>(const std::string_view&) const;
};

class KBVMultiplePrescription::Zeitraum : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "Zeitraum";

    friend std::optional<Zeitraum> ResourceBase::getExtension<Zeitraum>(const std::string_view&) const;
};

}

#endif// ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H
