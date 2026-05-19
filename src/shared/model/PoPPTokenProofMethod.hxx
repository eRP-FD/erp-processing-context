/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non_exclusively licensed to gematik GmbH
 */
#pragma once
#include <magic_enum/magic_enum.hpp>

enum class PoPPTokenProofMethod
{
    healthid,
    ehc_practitioner_trustedchannel,
    ehc_practitioner_cvc_authenticated,
    ehc_practitioner_user_x509,
    ehc_practitioner_owner_x509,
    ehc_provider_trustedchannel,
    ehc_provider_cvc_authenticated,
    ehc_provider_user_x509,
    ehc_provider_owner_x509
};

template<>
constexpr magic_enum::customize::customize_t
magic_enum::customize::enum_name<PoPPTokenProofMethod>(PoPPTokenProofMethod value) noexcept
{
    switch (value)
    {
        case PoPPTokenProofMethod::healthid:
            return "healthid";
        case PoPPTokenProofMethod::ehc_practitioner_trustedchannel:
            return "ehc-practitioner-trustedchannel";
        case PoPPTokenProofMethod::ehc_practitioner_cvc_authenticated:
            return "ehc-practitioner-cvc-authenticated";
        case PoPPTokenProofMethod::ehc_practitioner_user_x509:
            return "ehc-practitioner-user-x509";
        case PoPPTokenProofMethod::ehc_practitioner_owner_x509:
            return "ehc-practitioner-owner-x509";
        case PoPPTokenProofMethod::ehc_provider_trustedchannel:
            return "ehc-provider-trustedchannel";
        case PoPPTokenProofMethod::ehc_provider_cvc_authenticated:
            return "ehc-provider-cvc-authenticated";
        case PoPPTokenProofMethod::ehc_provider_user_x509:
            return "ehc-provider-user-x509";
        case PoPPTokenProofMethod::ehc_provider_owner_x509:
            return "ehc-provider-owner-x509";
    }
    return invalid_tag;
}

// Attention, the string representation of this enum is stored in the Audit Meta Data. Do not rename.
enum class PoPPTokenProofMethodPrefix
{
    healthid,
    ehc_practitioner,
    ehc_provider,
    unknown,
};

