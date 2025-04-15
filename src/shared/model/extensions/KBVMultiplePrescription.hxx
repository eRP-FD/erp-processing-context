/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H

#include "shared/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVMultiplePrescription : public Extension<KBVMultiplePrescription>
{
public:
    class Kennzeichen;
    class Nummerierung;
    class Zeitraum;
    class ID;

    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription";
    [[nodiscard]] bool isMultiplePrescription() const;
    [[nodiscard]] std::optional<std::string> mvoId() const;
    [[nodiscard]] std::optional<int> numerator() const;
    [[nodiscard]] std::optional<int> denominator() const;
    [[nodiscard]] std::optional<model::Timestamp> startDate() const;
    [[nodiscard]] std::optional<model::Timestamp> endDate() const;

    [[nodiscard]] std::optional<model::Timestamp> startDateTime() const;
    [[nodiscard]] std::optional<model::Timestamp> endDateTime() const;
};

class KBVMultiplePrescription::Kennzeichen : public model::Extension<Kennzeichen>
{
public:
    using Extension::Extension;
    static constexpr auto url = "Kennzeichen";
};

class KBVMultiplePrescription::Nummerierung : public model::Extension<Nummerierung>
{
public:
    using Extension::Extension;
    static constexpr auto url = "Nummerierung";
};

class KBVMultiplePrescription::Zeitraum : public model::Extension<Zeitraum>
{
public:
    using Extension::Extension;
    static constexpr auto url = "Zeitraum";
};

class KBVMultiplePrescription::ID : public model::Extension<ID>
{
public:
    using Extension::Extension;
    static constexpr auto url = "ID";
};

extern template class Extension<KBVMultiplePrescription>;
extern template class Extension<KBVMultiplePrescription::Kennzeichen>;
extern template class Extension<KBVMultiplePrescription::Nummerierung>;
extern template class Extension<KBVMultiplePrescription::Zeitraum>;
extern template class Extension<KBVMultiplePrescription::ID>;
extern template class Resource<KBVMultiplePrescription>;
extern template class Resource<KBVMultiplePrescription::Kennzeichen>;
extern template class Resource<KBVMultiplePrescription::Nummerierung>;
extern template class Resource<KBVMultiplePrescription::Zeitraum>;
extern template class Resource<KBVMultiplePrescription::ID>;
}

#endif// ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMULTIPLEPRESCRIPTION_H
