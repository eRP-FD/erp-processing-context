/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#pragma once
#include "shared/util/dosagetext/Language.hxx"
#include "shared/util/dosagetext/ScriptVersion.hxx"

#include <string>
#include <vector>

namespace model
{
class DosageDgMP;
}
namespace dosagetext
{

enum class Schema
{
    UNKNOWN,
    FREE_TEXT,
    FOUR_PATTERN,
    TIME_OF_DAY,
    DAY_OF_WEEK,
    INTERVAL,
    DAY_TIME_COMBO,
    INTERVAL_TIME_COMBO
};

class Renderer
{
public:
    Renderer(ScriptVersion version, Language language);
    virtual ~Renderer() = default;
    void render(const std::vector<model::DosageDgMP>& dosages);
    virtual std::string text() const = 0;
    virtual std::string separator() const;
    const ScriptVersion& scriptVersion() const;
    const Language& language() const;

    static bool is4Pattern(const std::vector<model::DosageDgMP>& dosages);
    static bool isFrequencyPattern(const std::vector<model::DosageDgMP>& dosages);
    static Schema detectSchema(const std::vector<model::DosageDgMP>& dosages);

private:
    virtual void doRender(const std::vector<model::DosageDgMP>& dosages) = 0;

    ScriptVersion mVersion;
    Language mLanguage;
};

}// dosagetext
