/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_COMMON_CONSTANTS_HXX
#define EPA_LIBRARY_COMMON_CONSTANTS_HXX

#include "library/util/MemoryUnits.hxx"

#include <cstdio>
#include <optional>


namespace epa
{

class Constants
{
public:
    static constexpr size_t DefaultBufferSize = 8192;

    // Maximum body size for any request body to the HttpsServer when used by the document manager.
    // 1.5 is a markup to account for metadata and xml protocol overhead and base64 encoding.
    // (we are assuming the 250 MB limit (Gematik actually means 250 MiB) refers to the actual raw
    // document size.)
    static constexpr auto maxBodySizeDocumentManager = static_cast<uint64_t>(1.5 * 250_MiB);

    // Maximum body size for any request body to the HttpsServer when used for record migration.
    // The current value means `unbounded`.
    static constexpr std::optional<uint64_t> maxBodySizeRecordMigration = std::nullopt;

    // Timeout in seconds by http connection
    static constexpr int httpTimeoutInSeconds = 30;

    // HTTP/1.1
    static constexpr int httpVersion_1_1 = 11;

    // The default upper bound of events to return from GetAuditEvents and GetSignedAuditEvents..
    // Values of 2000 are known to have caused bad_alloc errors when querying the signed audit log.
    static constexpr size_t auditLogEventConfigurationLimit = 500;
};

} // namespace epa

#endif
