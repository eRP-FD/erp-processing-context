/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/parser/FhirPathFixer.hxx"

namespace fhirtools
{

std::string_view FhirPathFixer::fixFhirPath(std::string_view fhirPath)
{
    // ref-1
    if (fhirPath == "reference.startsWith('#').not() or (reference.substring(1).trace('url') in "
                    "%rootResource.contained.id.trace('ids'))")
    {
        return "reference.exists() implies (reference = '#' or (reference.startsWith('#').not() or "
               "(reference.substring(1).trace('url') in %rootResource.contained.id.trace('ids'))))";
    }
    // bdl-8
    if (fhirPath == "fullUrl.contains('/_history/').not()")
    {
        return "fullUrl.exists() implies fullUrl.contains('/_history/').not()";
    }
    // nsd-0
    if (fhirPath == "name.matches('[A-Z]([A-Za-z0-9_]){0,254}')")
    {
        return "name.exists() implies name.matches('[A-Z]([A-Za-z0-9_]){0,254}')";
    }
    // vs-de-2
    if (fhirPath == "(component.empty() and hasMember.empty()) implies (dataAbsentReason or value)")
    {
        return "(component.empty() and hasMember.empty()) implies (dataAbsentReason.exists() or value.exists())";
    }
    return fhirPath;
}

} // namespace fhirtools