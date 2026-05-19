/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/ScriptVersion.hxx"

namespace dosagetext
{

std::string knownVersionsList()
{
    std::string result;
    for (auto scriptVersionId : magic_enum::enum_values<ScriptVersionId>())
    {
        if (! result.empty())
        {
            result.append(",");
        }
        switch (scriptVersionId)
        {
            case ScriptVersionId::V_1_0_1:
                result.append(VERSION_1_0_1.version);
                break;
        }
    }
    return result;
}
}
