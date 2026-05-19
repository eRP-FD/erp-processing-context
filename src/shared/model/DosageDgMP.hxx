/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#pragma once

#include "shared/model/Resource.hxx"

namespace model
{
// These are the CS values that are supported by the reference script. They do not match directly to a ValueSet
// https://github.com/hl7germany/dgMP-DosageTextgenerierung-Skript/blob/a9683825be784db107adbd4b5cd796ffd4764394/medication-dosage-to-text.py#L74-L73
enum class TimingDpMpTimeUnit
{
    s,
    min,
    h,
    d,
    wk,
    mo,
    a
};
bool isDaily(TimingDpMpTimeUnit timeUnit);
bool isWeekly(TimingDpMpTimeUnit timeUnit);

enum class DaysOfWeek
{
    mon,
    tue,
    wed,
    thu,
    fri,
    sat,
    sun
};

enum class EventTiming
{
    MORN,
    NOON,
    EVE,
    NIGHT
};

struct TimingDgMpBoundsDuration {
    explicit TimingDgMpBoundsDuration(const rapidjson::Value& boundsDurationValue);
    std::string value;
    std::string unit;
    std::string system;
    TimingDpMpTimeUnit code;
};

struct TimingDpMp {
    explicit TimingDpMp(const rapidjson::Value& timingValue);
    std::optional<TimingDgMpBoundsDuration> boundsDuration;
    std::string frequency;
    std::string period;
    TimingDpMpTimeUnit periodUnit;
    std::vector<DaysOfWeek> daysOfWeek;
    std::vector<std::string> timesOfDay;
    std::vector<EventTiming> when;
};

struct SimpleQuantity {
    explicit SimpleQuantity(const rapidjson::Value& quantityValue);
    std::string value;
    std::string unit;
    auto operator<=>(const SimpleQuantity&) const = default;
    bool operator==(const SimpleQuantity& other) const = default;
    bool operator!=(const SimpleQuantity& other) const = default;
};


// http://ig.fhir.de/igs/medication/StructureDefinition/DosageDgMP
class DosageDgMP : public Resource<DosageDgMP>
{
public:
    static constexpr auto resourceTypeName = "Dosage";
    [[nodiscard]] std::optional<std::string_view> text() const;
    [[nodiscard]] std::optional<TimingDpMp> timing() const;
    [[nodiscard]] std::optional<SimpleQuantity> doseQuantity() const;

private:
    friend Resource<DosageDgMP>;
    explicit DosageDgMP(NumberAsStringParserDocument&& document);
    mutable std::optional<std::string_view> mCachedText;
    mutable bool mCachedTextValid = false;
    mutable std::optional<TimingDpMp> mCachedTiming;
    mutable bool mCachedTimingValid = false;
    mutable std::optional<SimpleQuantity> mCachedDoseQuantity;
    mutable bool mCachedDoseQuantityValid = false;
};

std::vector<DosageDgMP> getDosageInstructionFromResource(const ResourceBase& resource);
std::vector<DosageDgMP> getDosageFromResource(const ResourceBase& resource);
std::vector<DosageDgMP> getDosageFromResource(const ResourceBase& resource, const rapidjson::Pointer& dosagePointer);

extern template class Resource<DosageDgMP>;

}// model
