// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

// Keep these names in sync with the configuration:
enum class DurationCategory
{
    redis,
    postgres,
    httpclient,
    fhirvalidation,
    ocsprequest,
    hsm,
    enrolment
};
