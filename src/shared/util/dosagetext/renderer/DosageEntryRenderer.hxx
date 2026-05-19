/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/util/dosagetext/renderer/Renderer.hxx"
#include "shared/model/DosageDgMP.hxx"

#include <map>

namespace dosagetext
{

class DosageEntryRenderer : public Renderer
{
public:
    DosageEntryRenderer(ScriptVersion version, Language language);
    std::string text() const override;
    std::string separator() const override;

    struct QuantityPerTime;
    struct QuantitiesPerTime;

private:
    void doRender(const std::vector<model::DosageDgMP>& dosages) override;
    static std::string
    render4Pattern(const std::map<std::optional<model::DaysOfWeek>, QuantitiesPerTime>& daysOfWeekQuanities);
    static std::string
    renderLinePattern(const std::map<std::optional<model::DaysOfWeek>, QuantitiesPerTime>& daysOfWeekQuanities,
                      bool shouldMergeTimes);
    static bool shouldMergeTimes(const std::vector<model::DosageDgMP>& dosages);
    std::string mText;
};

}// dosagetext
