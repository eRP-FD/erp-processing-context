/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/DosageDgMP.hxx"
#include "shared/util/dosagetext/renderer/Localization.hxx"

namespace model
{

DosageDgMP::DosageDgMP(NumberAsStringParserDocument&& document)
    : Resource<DosageDgMP>(std::move(document))
{
}

bool isDaily(TimingDpMpTimeUnit timeUnit)
{
    switch (timeUnit)
    {
        case TimingDpMpTimeUnit::d:
            return true;
        case TimingDpMpTimeUnit::s:
        case TimingDpMpTimeUnit::min:
        case TimingDpMpTimeUnit::h:
        case TimingDpMpTimeUnit::wk:
        case TimingDpMpTimeUnit::mo:
        case TimingDpMpTimeUnit::a:
            break;
    }
    return false;
}

bool isWeekly(TimingDpMpTimeUnit timeUnit)
{
    switch (timeUnit)
    {
        case TimingDpMpTimeUnit::wk:
            return true;
        case TimingDpMpTimeUnit::s:
        case TimingDpMpTimeUnit::min:
        case TimingDpMpTimeUnit::h:
        case TimingDpMpTimeUnit::d:
        case TimingDpMpTimeUnit::mo:
        case TimingDpMpTimeUnit::a:
            break;
    }
    return false;
}

TimingDgMpBoundsDuration::TimingDgMpBoundsDuration(const rapidjson::Value& boundsDurationValue)
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::value));
    static const rapidjson::Pointer unitPointer(resource::ElementName::path(resource::elements::unit));
    static const rapidjson::Pointer systemPointer(resource::ElementName::path(resource::elements::system));
    static const rapidjson::Pointer codePointer(resource::ElementName::path(resource::elements::code));
    ModelExpectNoThrow(value =
                           NumberAsStringParserDocument::getStringValueFromValue(valuePointer.Get(boundsDurationValue)),
                       "failed to parse boundDuration value");
    ModelExpectNoThrow(unit =
                           NumberAsStringParserDocument::getStringValueFromValue(unitPointer.Get(boundsDurationValue)),
                       "failed to parse boundDuration unit");
    ModelExpectNoThrow(
        system = NumberAsStringParserDocument::getStringValueFromValue(systemPointer.Get(boundsDurationValue)),
        "failed to parse boundDuration system");
    ModelExpectNoThrow(
        code = ::value(magic_enum::enum_cast<TimingDpMpTimeUnit>(
            NumberAsStringParserDocument::getStringValueFromValue(codePointer.Get(boundsDurationValue)))),
        "failed to parse boundDuration code");
}

TimingDpMp::TimingDpMp(const rapidjson::Value& timingValue)
{
    static const rapidjson::Pointer repeatBoundsDurationPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::boundsDuration));
    static const rapidjson::Pointer frequencyPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::frequency));
    static const rapidjson::Pointer periodPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::period));
    static const rapidjson::Pointer unitPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::periodUnit));
    static const rapidjson::Pointer dayOfWeekPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::dayOfWeek));
    static const rapidjson::Pointer timeOfDayPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::timeOfDay));
    static const rapidjson::Pointer whenPointer(
        resource::ElementName::path(resource::elements::repeat, resource::elements::when));
    if (const auto* boundsDurationValue = repeatBoundsDurationPointer.Get(timingValue))
    {
        boundsDuration = TimingDgMpBoundsDuration(*boundsDurationValue);
    }
    ModelExpectNoThrow(frequency =
                           NumberAsStringParserDocument::getStringValueFromValue(frequencyPointer.Get(timingValue)),
                       "failed to parse frequency");
    ModelExpectNoThrow(period = dosagetext::germanDecimal(
                           NumberAsStringParserDocument::getStringValueFromValue(periodPointer.Get(timingValue))),
                       "failed to parse period");
    ModelExpectNoThrow(periodUnit = ::value(magic_enum::enum_cast<TimingDpMpTimeUnit>(
                           NumberAsStringParserDocument::getStringValueFromValue(unitPointer.Get(timingValue)))),
                       "failed to parse unit");
    if (const auto* dayOfWeekArray = dayOfWeekPointer.Get(timingValue))
    {
        ModelExpect(dayOfWeekArray->IsArray(), "dayOfWeek is not an array");
        for (const auto& dayOfWeek : dayOfWeekArray->GetArray())
        {
            ModelExpectNoThrow(daysOfWeek.emplace_back(::value(magic_enum::enum_cast<DaysOfWeek>(
                                   NumberAsStringParserDocument::getStringValueFromValue(&dayOfWeek)))),
                               "failed to parse dayOfWeek");
        }
        std::ranges::sort(daysOfWeek);
    }
    if (const auto* timeOfDayArray = timeOfDayPointer.Get(timingValue))
    {
        ModelExpect(timeOfDayArray->IsArray(), "timeOfDay is not an array");
        for (const auto& time : timeOfDayArray->GetArray())
        {
            ModelExpectNoThrow(timesOfDay.emplace_back(NumberAsStringParserDocument::getStringValueFromValue(&time)),
                               "failed to parse timeOfDay");
        }
        std::ranges::sort(timesOfDay);
    }
    if (const auto* whenArray = whenPointer.Get(timingValue))
    {
        ModelExpect(whenArray->IsArray(), "when is not an array");
        for (const auto& when : whenArray->GetArray())
        {
            ModelExpectNoThrow(this->when.emplace_back(::value(magic_enum::enum_cast<EventTiming>(
                                   NumberAsStringParserDocument::getStringValueFromValue(&when)))),
                               "failed to parse when");
        }
        std::ranges::sort(when);
    }
}

SimpleQuantity::SimpleQuantity(const rapidjson::Value& quantityValue)
{
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::value));
    static const rapidjson::Pointer unitPointer(resource::ElementName::path(resource::elements::unit));
    ModelExpectNoThrow(value = dosagetext::germanDecimal(
                           NumberAsStringParserDocument::getStringValueFromValue(valuePointer.Get(quantityValue))),
                       "failed to parse quantity value");
    ModelExpectNoThrow(unit = NumberAsStringParserDocument::getStringValueFromValue(unitPointer.Get(quantityValue)),
                       "failed to parse quantity unit");
}

std::optional<std::string_view> DosageDgMP::text() const
{
    static const rapidjson::Pointer textPointer(resource::ElementName::path(resource::elements::text));
    if (! mCachedTextValid)
    {
        mCachedText = getOptionalStringValue(textPointer);
        mCachedTextValid = true;
    }
    return mCachedText;
}

std::optional<TimingDpMp> DosageDgMP::timing() const
{
    static const rapidjson::Pointer timingPointer(resource::ElementName::path(resource::elements::timing));
    if (! mCachedTimingValid)
    {
        if (const auto* timing = getValue(timingPointer))
        {
            mCachedTiming = TimingDpMp(*timing);
        }
        mCachedTimingValid = true;
    }
    return mCachedTiming;
}

std::optional<SimpleQuantity> DosageDgMP::doseQuantity() const
{
    static const rapidjson::Pointer doseQuantityPointer(
        resource::ElementName::path(resource::elements::doseAndRate, 0, resource::elements::doseQuantity));
    if (! mCachedDoseQuantityValid)
    {
        if (const auto* doseQuantity = getValue(doseQuantityPointer))
        {
            mCachedDoseQuantity = SimpleQuantity(*doseQuantity);
        }
        mCachedDoseQuantityValid = true;
    }
    return mCachedDoseQuantity;
}

std::vector<DosageDgMP> getDosageInstructionFromResource(const ResourceBase& resource)
{
    static const rapidjson::Pointer dosageInstructionPointer(resource::ElementName::path("dosageInstruction"));
    return getDosageFromResource(resource, dosageInstructionPointer);
}

std::vector<DosageDgMP> getDosageFromResource(const ResourceBase& resource)
{
    static const rapidjson::Pointer dosagePointer(resource::ElementName::path("dosage"));
    return getDosageFromResource(resource, dosagePointer);
}

std::vector<DosageDgMP> getDosageFromResource(const ResourceBase& resource, const rapidjson::Pointer& dosagePointer)
{
    const auto* array = dosagePointer.Get(resource.jsonDocument());
    if (!array)
    {
        return {};
    }
    ModelExpect(array->IsArray(), "dosage/dosageInstruction must be array of object");
    std::vector<DosageDgMP> result;
    for (const auto& entry : array->GetArray())
    {
        result.emplace_back(DosageDgMP::fromJson(entry));
    }
    return result;
}

}