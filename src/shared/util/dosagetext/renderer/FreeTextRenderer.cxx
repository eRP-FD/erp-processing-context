/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/renderer/FreeTextRenderer.hxx"
#include "shared/model/DosageDgMP.hxx"

namespace dosagetext
{

std::string FreeTextRenderer::text() const
{
    return mText;
}

std::string FreeTextRenderer::separator() const
{
    return "";
}

void FreeTextRenderer::doRender(const std::vector<model::DosageDgMP>& dosages)
{
    for (const auto& dosage : dosages)
    {
        if (! mText.empty())
        {
            mText.append("; ");
        }
        if (dosage.text().has_value())
        {
            mText.append(*dosage.text());
        }
    }
}

}
