/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/dosagetext/renderer/BoundsDurationRenderer.hxx"
#include "shared/util/dosagetext/renderer/Localization.hxx"
#include "shared/model/DosageDgMP.hxx"

namespace dosagetext
{

void BoundsDurationRenderer::doRender(const std::vector<model::DosageDgMP>& dosages)
{
    // _extract_bounds_text in the reference script
    // https://github.com/hl7germany/dgMP-DosageTextgenerierung-Skript/blob/a9683825be784db107adbd4b5cd796ffd4764394/medication-dosage-to-text.py#L471-L470
    // Extract duration bounds (should be consistent across all dosages)
    if (const auto timing = dosages.at(0).timing())
    {
        if (timing->boundsDuration.has_value())
        {
            const auto value = germanDecimal(timing->boundsDuration->value);
            mText.append("für ")
                .append(value)
                .append(" ")
                .append(germanTimeUnit(timing->boundsDuration->code, value == "1"
                                                                 ? GrammaticalNumber::SINGULAR
                                                                 : GrammaticalNumber::PLURAL));
        }
    }
}

std::string BoundsDurationRenderer::text() const
{
    return mText;
}

}
