/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

/// @brief Standard X.509 certificate policy OIDs from public PKI specifications.
///
/// These are non-gematik OIDs. Gematik-specific OIDs are in TslService.hxx.
namespace certificate_policy
{
// GEMREQ-start A_28933
/// Extended Validation TLS
static constexpr const char* oid_ev_tls{"2.23.140.1.1"};

/// Organization Validated TLS
static constexpr const char* oid_ov_tls{"2.23.140.1.2.2"};
// GEMREQ-end A_28933
}
