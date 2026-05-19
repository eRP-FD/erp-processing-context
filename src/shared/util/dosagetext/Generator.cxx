/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/Generator.hxx"
#include "renderer/BoundsDurationRenderer.hxx"
#include "renderer/DosageEntryRenderer.hxx"
#include "renderer/FreeTextRenderer.hxx"
#include "renderer/FrequencyPeriodRenderer.hxx"
#include "renderer/Localization.hxx"
#include "renderer/Renderer.hxx"
#include "shared/model/DosageDgMP.hxx"

#include <list>
#include <memory>
#include <utility>

namespace dosagetext
{

Generator::Generator(ScriptVersion version, Language language)
    : mVersion(std::move(version))
    , mLanguage(std::move(language))
{
    switch (mVersion.versionId)
    {
        case ScriptVersionId::V_1_0_1:
            break;
    }
}

RenderedDosageInstruction Generator::render(const std::vector<model::DosageDgMP>& dosages) const
{

    if (Renderer::detectSchema(dosages) != Schema::UNKNOWN)
    {
        try
        {
            // [Dauer] [Intervall / Grundrhythmus]: [optionale Einschränkung auf Wochentage] [Zeit- oder Tagesabschnittsangaben] [Dosisangabe]
            std::list<std::unique_ptr<Renderer>> v1_0_1_renderers;
            v1_0_1_renderers.emplace_back(std::make_unique<BoundsDurationRenderer>(mVersion, mLanguage));
            v1_0_1_renderers.emplace_back(std::make_unique<FrequencyPeriodRenderer>(mVersion, mLanguage));
            v1_0_1_renderers.emplace_back(std::make_unique<DosageEntryRenderer>(mVersion, mLanguage));
            v1_0_1_renderers.emplace_back(std::make_unique<FreeTextRenderer>(mVersion, mLanguage));

            std::string rendered;
            std::string separator;
            for (const auto& renderer : v1_0_1_renderers)
            {
                renderer->render(dosages);
                const auto& text = renderer->text();
                if (! text.empty())
                {
                    rendered.append(separator).append(text);
                }
                if (! rendered.empty() || ! separator.empty())
                {
                    separator = renderer->separator();
                }
            }

            return {.renderedDosageInstruction = std::move(rendered), .scriptVersion = mVersion, .language = mLanguage};
        }
        catch (const std::exception& e)
        {
            TVLOG(1) << "caught exception during rendering of dosage instruction text: " << e.what();
            // fallthrough
        }
    }
    return {.renderedDosageInstruction = std::string{englishInvalidOrUnsupported()},
            .scriptVersion = mVersion,
            .language = mLanguage};
}

}
